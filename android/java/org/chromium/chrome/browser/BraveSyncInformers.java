/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.chromium.chrome.browser;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;

import org.chromium.base.ContextUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.BraveSyncWorker;
import org.chromium.chrome.browser.app.BraveActivity;
import org.chromium.chrome.browser.infobar.InfoBarIdentifier;
import org.chromium.chrome.browser.preferences.BravePrefServiceBridge;
import org.chromium.chrome.browser.settings.BraveSyncScreensPreference;
import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.infobar.BraveSimpleConfirmInfoBarBuilder;
import org.chromium.chrome.browser.ui.messages.infobar.SimpleConfirmInfoBarBuilder;

public class BraveSyncInformers {
    public static void show() {
        showSetupV2IfRequired();
    }

    private static void showSetupV2IfRequired() {
        if (null == BraveSyncReflectionUtils.getSyncWorker()) return;

        BraveSyncWorker braveSyncWorker = (BraveSyncWorker)BraveSyncReflectionUtils.getSyncWorker();
        boolean wasV1User = braveSyncWorker.getSyncV1WasEnabled();
        if (!wasV1User) {
            return;
        }

        boolean infobarDismissed = braveSyncWorker.getSyncV2MigrateNoticeDismissed();
        if (infobarDismissed) {
            return;
        }

        boolean isV2User = ProfileSyncService.get() != null && ProfileSyncService.get().isFirstSetupComplete();
        if (isV2User) {
            braveSyncWorker.setSyncV2MigrateNoticeDismissed(true);
            return;
        }

        showSyncV2NeedsSetup();
    }

    public static void showSyncV2NeedsSetup() {
        BraveActivity activity = BraveActivity.getBraveActivity();
        if (activity == null) return;

        Tab tab = activity.getActivityTabProvider().get();
        if (tab == null) return;

        BraveSimpleConfirmInfoBarBuilder.createInfobarWithDrawable(tab.getWebContents(),
                new SimpleConfirmInfoBarBuilder.Listener() {
                    @Override
                    public void onInfoBarDismissed() {
                    }

                    @Override
                    public boolean onInfoBarButtonClicked(boolean isPrimary) {
                        if (isPrimary) {
                            SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
                            settingsLauncher.launchSettingsActivity(
                                ContextUtils.getApplicationContext(),
                                BraveSyncScreensPreference.class);
                        }
                        return false;
                    }

                    @Override
                    public boolean onInfoBarLinkClicked() {
                        return false;
                    }
                },
                // must be SYNC_V2_MIGRATE_INFOBAR_DELEGATE, but now it is introduced through
                // src/brave/chromium_src/components/infobars/core/infobar_delegate.h and
                // java enums are generated by //components/infobars/core:infobar_generated_enums
                // who does not understand `brave/chromium_src`
                InfoBarIdentifier.INLINE_UPDATE_READY_INFOBAR_ANDROID,
                activity,
                R.drawable.sync_icon /* drawableId */,
                activity.getString(R.string.brave_sync_v2_migrate_infobar_message) /* message */,
                activity.getString(R.string.brave_sync_v2_migrate_infobar_command) /* primaryText */,
                null /* secondaryText */, null /* linkText */, false /* autoExpire */);
        BraveSyncWorker braveSyncWorker = (BraveSyncWorker)BraveSyncReflectionUtils.getSyncWorker();
        braveSyncWorker.setSyncV2MigrateNoticeDismissed(true);
    }
}
