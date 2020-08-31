/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import * as React from 'react'

import { TwitterColorIcon, RedditColorIcon } from 'brave-ui/components/icons'

import { Host, TipKind } from '../lib/interfaces'
import { HostContext } from '../lib/host_context'

import { SliderSwitch, SliderSwitchOption } from './slider_switch'
import { TipComplete } from './tip_complete'
import { OneTimeTipForm } from './one_time_tip_form'
import { MonthlyTipForm } from './monthly_tip_form'

import * as style from './tip_form.style'

function getTipKindOptions (
  host: Host,
  showMonthlyStar: boolean
): SliderSwitchOption<TipKind>[] {
  let monthlyText = host.getString('monthlyText')
  if (showMonthlyStar) {
    monthlyText = '* ' + monthlyText
  }
  return [
    { value: 'one-time', content: host.getString('oneTimeTip') },
    { value: 'monthly', content: monthlyText }
  ]
}

function getHeaderSubtitle (host: Host) {
  const { mediaMetaData } = host.getDialogArgs()

  if (mediaMetaData.mediaType === 'twitter') {
    return (
      <style.headerSubtitle>
        {host.getString('tipPostSubtitle')}&nbsp;
        <style.socialIcon>
          <TwitterColorIcon />
        </style.socialIcon>
      </style.headerSubtitle>
    )
  }

  if (mediaMetaData.mediaType === 'reddit') {
    return (
      <style.headerSubtitle>
        {host.getString('tipPostSubtitle')}&nbsp;
        <style.socialIcon><RedditColorIcon /></style.socialIcon>
      </style.headerSubtitle>
    )
  }

  return ''
}

export function TipForm () {
  const host = React.useContext(HostContext)
  const { getString } = host

  const [balanceInfo, setBalanceInfo] = React.useState(host.state.balanceInfo)
  const [publisherInfo, setPublisherInfo] = React.useState(host.state.publisherInfo)
  const [rewardsParameters, setRewardsParameters] = React.useState(host.state.rewardsParameters)
  const [currentMonthlyTip, setCurrentMonthlyTip] = React.useState(host.state.currentMonthlyTip || 0)

  const [tipAmount, setTipAmount] = React.useState(0)
  const [tipProcessed, setTipProcessed] = React.useState(false)
  const [wasMonthlySelected, setWasMonthlySelected] = React.useState(false)

  const [tipKind, setTipKind] = React.useState<TipKind>(() => {
    const { entryPoint } = host.getDialogArgs()
    return entryPoint === 'one-time' ? 'one-time' : 'monthly'
  })

  React.useEffect(() => {
    return host.addListener((state) => {
      setTipProcessed(Boolean(state.tipProcessed))
      setTipAmount(state.tipAmount || 0)
      setRewardsParameters(state.rewardsParameters)
      setPublisherInfo(state.publisherInfo)
      setBalanceInfo(state.balanceInfo)
      setCurrentMonthlyTip(state.currentMonthlyTip || 0)
    })
  }, [host])

  if (!rewardsParameters || !publisherInfo || !balanceInfo) {
    return <style.loading />
  }

  if (tipProcessed) {
    return <TipComplete tipKind={tipKind} tipAmount={tipAmount} />
  }

  function onTipKindSelect (value: TipKind) {
    if (value === 'monthly') {
      setWasMonthlySelected(true)
    }
    setTipKind(value)
  }

  function getTipKindSelector () {
    const { mediaType } = host.getDialogArgs().mediaMetaData
    if (mediaType === 'twitter' || mediaType === 'reddit') {
      return null
    }

    const showMonthlyIndicator =
      !wasMonthlySelected &&
      host.getDialogArgs().entryPoint === 'one-time' &&
      currentMonthlyTip > 0

    return (
      <>
        {
          showMonthlyIndicator &&
            <style.monthlyIndicator>
              <style.monthlyIndicatorStar>*</style.monthlyIndicatorStar>&nbsp;
              {getString('currentlySupporting')}
            </style.monthlyIndicator>
        }
        <SliderSwitch<TipKind>
          options={getTipKindOptions(host, showMonthlyIndicator)}
          selectedValue={tipKind}
          onSelect={onTipKindSelect}
        />
      </>
    )
  }

  return (
    <style.root>
      <style.header>
        {getString('supportThisCreator')}
        {getHeaderSubtitle(host)}
      </style.header>
      <style.tipKind>
        {getTipKindSelector()}
      </style.tipKind>
      <style.main>
        {tipKind === 'one-time' ? <OneTimeTipForm /> : <MonthlyTipForm />}
      </style.main>
    </style.root>
  )
}
