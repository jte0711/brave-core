/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const path = require('path')
const config = require('../lib/config')
const util = require('../lib/util')
const {rebaseBraveStringFilesOnChromiumL10nFiles, braveAutoGeneratedPaths, logRemovedGRDParts} = require('./l10nUtil')

const resetChromeStringFiles = () => {
  // Revert to originals before string replacement because original grd(p)s are
  // overwritten with modified versions from ./src/brave during build.
  const srcDir = config.projects['chrome'].dir
  const targetFilesForReset = [ "*.grd", "*.grdp" ]
  targetFilesForReset.forEach((targetFile) => {
    util.run('git', ['checkout', '--', targetFile], { cwd: srcDir })
  })
}

const chromiumRebaseL10n = async (options) => {
  resetChromeStringFiles()
  const removed = await rebaseBraveStringFilesOnChromiumL10nFiles()
  braveAutoGeneratedPaths.forEach((sourceStringPath) => {
    const cmdOptions = config.defaultOptions
    cmdOptions.cwd = config.projects['brave-core'].dir
    util.run('python', ['script/chromium-rebase-l10n.py', '--source_string_path', sourceStringPath], cmdOptions)
  })
  logRemovedGRDParts(removed)
}

module.exports = chromiumRebaseL10n