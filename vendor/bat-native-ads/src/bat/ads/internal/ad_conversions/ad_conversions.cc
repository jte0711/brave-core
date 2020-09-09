/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/ad_conversions/ad_conversions.h"

#include <algorithm>
#include <functional>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "brave_base/random.h"
#include "bat/ads/internal/ads_impl.h"
#include "bat/ads/internal/confirmations/confirmations.h"
#include "bat/ads/internal/database/tables/ad_conversions_database_table.h"
#include "bat/ads/internal/filters/ads_history_filter_factory.h"
#include "bat/ads/internal/logging.h"
#include "bat/ads/internal/sorts/ad_conversions/ad_conversions_sort_factory.h"
#include "bat/ads/internal/sorts/ads_history/ads_history_sort_factory.h"
#include "bat/ads/internal/time_util.h"
#include "bat/ads/internal/url_util.h"

namespace ads {

using std::placeholders::_1;
using std::placeholders::_2;

namespace {

const char kAdConversionsFilename[] = "ad_conversions.json";

const char kAdConversionsListKey[] = "ad_conversions";
const char kAdConversionTimestampKey[] = "timestamp_in_seconds";
const char kAdConversionCreativeSetIdKey[] = "creative_set_id";
const char kAdConversionCreativeInstanceeIdKey[] = "uuid";

const int kAdConversionFrequency =
    base::Time::kHoursPerDay * base::Time::kSecondsPerHour;
const int kDebugAdConversionFrequency = 10 * base::Time::kSecondsPerMinute;
const int kExpiredAdConversionFrequency = 5 * base::Time::kSecondsPerMinute;

}  // namespace

AdConversions::AdConversions(
    AdsImpl* ads)
    : is_initialized_(false),
      ads_(ads) {
  DCHECK(ads_);
}

AdConversions::~AdConversions() = default;

void AdConversions::Initialize(
    InitializeCallback callback) {
  callback_ = callback;

  Load();
}

void AdConversions::MaybeConvert(
    const std::string& url) {
  DCHECK(is_initialized_);

  if (!ads_->get_ads_client()->ShouldAllowAdConversionTracking()) {
    BLOG(1, "Ad conversions are not allowed");
    return;
  }

  if (!UrlHasScheme(url)) {
    BLOG(1, "Visited URL is not supported for ad conversions");
    return;
  }

  BLOG(1, "Checking visited URL for ad conversions");

  database::table::AdConversions database_table(ads_);
  database_table.Get(std::bind(&AdConversions::OnGetAdConversions, this,
      url, _1, _2));
}

void AdConversions::StartTimerIfReady() {
  DCHECK(is_initialized_);

  if (timer_.IsRunning()) {
    return;
  }

  if (queue_.empty()) {
    BLOG(1, "Ad conversion queue is empty");
    return;
  }

  AdConversionQueueItemInfo ad_conversion = queue_.front();
  StartTimer(ad_conversion);
}

///////////////////////////////////////////////////////////////////////////////

void AdConversions::OnGetAdConversions(
    const std::string& url,
    const Result result,
    const AdConversionList& ad_conversions) {
  if (result != SUCCESS) {
    BLOG(1, "No ad conversions found");
    return;
  }

  std::deque<AdHistory> ads_history = ads_->get_client()->GetAdsHistory();
  ads_history = FilterAdsHistory(ads_history);
  ads_history = SortAdsHistory(ads_history);

  AdConversionList new_ad_conversions = ad_conversions;
  new_ad_conversions = FilterAdConversions(url, new_ad_conversions);
  new_ad_conversions = SortAdConversions(new_ad_conversions);

  bool converted = false;

  for (const auto& ad_conversion : new_ad_conversions) {
    for (const auto& ad : ads_history) {
      auto ad_conversion_history = ads_->get_client()->GetAdConversionHistory();
      if (ad_conversion_history.find(ad_conversion.creative_set_id) !=
          ad_conversion_history.end()) {
        // Creative set id has already been converted
        continue;
      }

      if (ad_conversion.creative_set_id != ad.ad_content.creative_set_id) {
        // Creative set id does not match
        continue;
      }

      const base::Time observation_window = base::Time::Now() -
          base::TimeDelta::FromDays(ad_conversion.observation_window);
      const base::Time time = base::Time::FromDoubleT(ad.timestamp_in_seconds);
      if (observation_window > time) {
        // Observation window has expired
        continue;
      }

      BLOG(1, "Ad conversion for creative set id " <<
          ad_conversion.creative_set_id << " and "
              << std::string(ad_conversion.type));

      AddItemToQueue(ad.ad_content.creative_instance_id,
          ad.ad_content.creative_set_id);

      converted = true;
    }
  }

  if (!converted) {
    BLOG(1, "No ad conversion matches found for visited URL");
  }
}

std::deque<AdHistory> AdConversions::FilterAdsHistory(
    const std::deque<AdHistory>& ads_history) {
  const auto filter = AdsHistoryFilterFactory::Build(
      AdsHistory::FilterType::kAdConversion);
  DCHECK(filter);

  return filter->Apply(ads_history);
}

std::deque<AdHistory> AdConversions::SortAdsHistory(
    const std::deque<AdHistory>& ads_history) {
  const auto sort = AdsHistorySortFactory::Build(
      AdsHistory::SortType::kDescendingOrder);
  DCHECK(sort);

  return sort->Apply(ads_history);
}

AdConversionList AdConversions::FilterAdConversions(
    const std::string& url,
    const AdConversionList& ad_conversions) {
  AdConversionList new_ad_conversions = ad_conversions;
  const auto iter = std::remove_if(new_ad_conversions.begin(),
      new_ad_conversions.end(), [&](const AdConversionInfo& info) {
    return !UrlMatchesPattern(url, info.url_pattern);
  });
  new_ad_conversions.erase(iter, new_ad_conversions.end());

  return new_ad_conversions;
}

AdConversionList AdConversions::SortAdConversions(
    const AdConversionList& ad_conversions) {
  const auto sort = AdConversionsSortFactory::Build(
      AdConversionInfo::SortType::kDescendingOrder);
  DCHECK(sort);

  return sort->Apply(ad_conversions);
}

void AdConversions::AddItemToQueue(
    const std::string& creative_instance_id,
    const std::string& creative_set_id) {
  DCHECK(is_initialized_);
  DCHECK(!creative_instance_id.empty());
  DCHECK(!creative_set_id.empty());

  if (creative_instance_id.empty() || creative_set_id.empty()) {
    return;
  }

  ads_->get_client()->AppendAdConversionHistoryForCreativeSetId(
        creative_set_id);

  AdConversionQueueItemInfo ad_conversion;

  const uint64_t rand_delay = brave_base::random::Geometric(
      _is_debug ? kDebugAdConversionFrequency : kAdConversionFrequency);

  const uint64_t now = static_cast<uint64_t>(base::Time::Now().ToDoubleT());

  ad_conversion.timestamp_in_seconds = now + rand_delay;
  ad_conversion.creative_instance_id = creative_instance_id;
  ad_conversion.creative_set_id = creative_set_id;

  queue_.push_back(ad_conversion);

  std::sort(queue_.begin(), queue_.end(), [](
      const AdConversionQueueItemInfo& a,
      const AdConversionQueueItemInfo& b) {
    return a.timestamp_in_seconds < b.timestamp_in_seconds;
  });

  Save();

  StartTimerIfReady();
}

bool AdConversions::RemoveItemFromQueue(
    const std::string& creative_instance_id) {
  DCHECK(is_initialized_);

  auto iter = std::find_if(queue_.begin(), queue_.end(),
      [&creative_instance_id] (const auto& ad_conversion) {
    return ad_conversion.creative_instance_id == creative_instance_id;
  });

  if (iter == queue_.end()) {
    return false;
  }

  queue_.erase(iter);

  Save();

  return true;
}

void AdConversions::ProcessQueueItem(
    const AdConversionQueueItemInfo& queue_item) {
  const std::string friendly_date_and_time =
      FriendlyDateAndTime(queue_item.timestamp_in_seconds);

  if (!queue_item.IsValid()) {
    BLOG(1, "Failed to convert ad with creative instance id "
        << queue_item.creative_instance_id << " and creative set id "
            << queue_item.creative_set_id << " " << friendly_date_and_time);
  } else {
    const std::string creative_set_id = queue_item.creative_set_id;
    const std::string creative_instance_id = queue_item.creative_instance_id;

    BLOG(1, "Successfully converted ad with creative instance id "
        << queue_item.creative_instance_id << " and creative set id "
            << queue_item.creative_set_id << " " << friendly_date_and_time);

    AdInfo ad;
    ad.creative_instance_id = creative_instance_id;
    ad.creative_set_id = creative_set_id;

    ads_->get_confirmations()->ConfirmAd(ad, ConfirmationType::kConversion);
  }

  RemoveItemFromQueue(queue_item.creative_instance_id);

  StartTimerIfReady();
}

void AdConversions::ProcessQueue() {
  if (queue_.empty()) {
    return;
  }

  AdConversionQueueItemInfo ad_conversion = queue_.front();
  ProcessQueueItem(ad_conversion);
}

void AdConversions::StartTimer(
    const AdConversionQueueItemInfo& info) {
  DCHECK(is_initialized_);
  DCHECK(!timer_.IsRunning());

  const base::Time now = base::Time::Now();
  const base::Time timestamp =
    base::Time::FromDoubleT(info.timestamp_in_seconds);

  base::TimeDelta delay;
  if (now < timestamp) {
    delay = timestamp - now;
  } else {
    const uint64_t rand_delay = brave_base::random::Geometric(
        kExpiredAdConversionFrequency);
    delay = base::TimeDelta::FromSeconds(rand_delay);
  }

  const base::Time time = timer_.Start(delay,
      base::BindOnce(&AdConversions::ProcessQueue, base::Unretained(this)));

  BLOG(1, "Started ad conversion timer for creative instance id "
      << info.creative_instance_id << " and creative set id "
          << info.creative_set_id << " which will trigger "
              << FriendlyDateAndTime(time));
}

void AdConversions::Save() {
  if (!is_initialized_) {
    return;
  }

  BLOG(9, "Saving ad conversions state");

  std::string json = ToJson();
  auto callback = std::bind(&AdConversions::OnSaved, this, _1);
  ads_->get_ads_client()->Save(kAdConversionsFilename, json, callback);
}

void AdConversions::OnSaved(
    const Result result) {
  if (result != SUCCESS) {
    BLOG(0, "Failed to save ad conversions state");
    return;
  }

  BLOG(9, "Successfully saved ad conversions state");
}

std::string AdConversions::ToJson() {
  base::Value dictionary(base::Value::Type::DICTIONARY);

  auto ad_conversions = GetAsList();
  dictionary.SetKey(kAdConversionsListKey,
      base::Value(std::move(ad_conversions)));

  // Write to JSON
  std::string json;
  base::JSONWriter::Write(dictionary, &json);

  return json;
}

base::Value AdConversions::GetAsList() {
  base::Value list(base::Value::Type::LIST);

  for (const auto& ad_conversion : queue_) {
    base::Value dictionary(base::Value::Type::DICTIONARY);

    dictionary.SetKey(kAdConversionTimestampKey,
        base::Value(std::to_string(ad_conversion.timestamp_in_seconds)));
    dictionary.SetKey(kAdConversionCreativeInstanceeIdKey,
        base::Value(ad_conversion.creative_instance_id));
    dictionary.SetKey(kAdConversionCreativeSetIdKey,
        base::Value(ad_conversion.creative_set_id));

    list.Append(std::move(dictionary));
  }

  return list;
}

void AdConversions::Load() {
  BLOG(3, "Loading ad conversions state");

  auto callback = std::bind(&AdConversions::OnLoaded, this, _1, _2);
  ads_->get_ads_client()->Load(kAdConversionsFilename, callback);
}

void AdConversions::OnLoaded(
    const Result result,
    const std::string& json) {
  if (result != SUCCESS) {
    BLOG(3, "Ad conversions state does not exist, creating default state");

    is_initialized_ = true;

    queue_.clear();
    Save();
  } else {
    if (!FromJson(json)) {
      BLOG(0, "Failed to load ad conversions state");

      BLOG(3, "Failed to parse ad conversions state: " << json);

      callback_(FAILED);
      return;
    }

    BLOG(3, "Successfully loaded ad conversions state");

    is_initialized_ = true;
  }

  callback_(SUCCESS);
}

bool AdConversions::FromJson(
    const std::string& json) {
  base::Optional<base::Value> value = base::JSONReader::Read(json);
  if (!value || !value->is_dict()) {
    return false;
  }

  base::DictionaryValue* dictionary = nullptr;
  if (!value->GetAsDictionary(&dictionary)) {
    return false;
  }

  auto* ad_conversions_list_value = dictionary->FindKey(kAdConversionsListKey);
  if (!ad_conversions_list_value || !ad_conversions_list_value->is_list()) {
    return false;
  }

  base::ListValue* list = nullptr;
  if (!ad_conversions_list_value->GetAsList(&list)) {
    return false;
  }

  queue_ = GetFromList(list);

  Save();

  return true;
}

AdConversionQueueItemList AdConversions::GetFromList(
    const base::ListValue* list) const {
  AdConversionQueueItemList ad_conversions;

  DCHECK(list);
  if (!list) {
    return ad_conversions;
  }

  for (const auto& value : list->GetList()) {
    if (!value.is_dict()) {
      NOTREACHED();
      continue;
    }

    const base::DictionaryValue* dictionary = nullptr;
    value.GetAsDictionary(&dictionary);
    if (!dictionary) {
      NOTREACHED();
      continue;
    }

    AdConversionQueueItemInfo ad_conversion;
    if (!GetFromDictionary(dictionary, &ad_conversion)) {
      NOTREACHED();
      continue;
    }

    ad_conversions.push_back(ad_conversion);
  }

  return ad_conversions;
}

bool AdConversions::GetFromDictionary(
    const base::DictionaryValue* dictionary,
    AdConversionQueueItemInfo* info) const {
  DCHECK(dictionary);
  if (!dictionary) {
    return false;
  }

  DCHECK(info);
  if (!info) {
    return false;
  }

  AdConversionQueueItemInfo ad_conversion;

  const auto* timestamp = dictionary->FindStringKey(kAdConversionTimestampKey);
  if (!timestamp) {
    return false;
  }
  if (!base::StringToUint64(*timestamp, &ad_conversion.timestamp_in_seconds)) {
    return false;
  }

  // Creative Set Id
  const auto* creative_set_id =
      dictionary->FindStringKey(kAdConversionCreativeSetIdKey);
  if (!creative_set_id) {
    return false;
  }
  ad_conversion.creative_set_id = *creative_set_id;

  // UUID
  const auto* creative_instance_id =
      dictionary->FindStringKey(kAdConversionCreativeInstanceeIdKey);
  if (!creative_instance_id) {
    return false;
  }
  ad_conversion.creative_instance_id = *creative_instance_id;

  *info = ad_conversion;

  return true;
}

}  // namespace ads
