/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import * as React from 'react'

import { EmoteSadIcon } from 'brave-ui/components/icons'

import {
  Host,
  PaymentKind,
  RewardsParameters,
  PublisherInfo
} from '../lib/interfaces'

import { HostContext } from '../lib/host_context'
import { formatExchangeAmount, formatLocaleTemplate } from '../lib/formatting'

import { FormSubmitButton } from './form_submit_button'
import { PaymentKindSwitch } from './payment_kind_switch'
import { TipAmountSelector } from './tip_amount_selector'
import { BatString } from './bat_string'
import { TermsOfService } from './terms_of_service'
import { NewTabLink } from './new_tab_link'
import { PaperAirplaneIcon } from './icons/paper_airplane_icon'

import * as style from './one_time_tip_form.style'

function generateTipOptions (
  rewardsParameters: RewardsParameters,
  publisherInfo: PublisherInfo
) {
  const publisherAmounts = publisherInfo.amounts
  if (publisherAmounts && publisherAmounts.length > 0) {
    return publisherAmounts
  }
  const { tipChoices } = rewardsParameters
  if (tipChoices.length > 0) {
    return tipChoices
  }
  return [1, 5, 10]
}

function getDefaultTipOption (options: number[], balance: number) {
  if (options.length > 0) {
    // Select the highest amount that is greater than or equal
    // to the user's balance, starting from the middle option.
    for (let i = Math.floor(options.length / 2); i >= 0; --i) {
      if (i === 0 || options[i] <= balance) {
        return options[i]
      }
    }
  }
  return 0
}

function getInsufficientFundsContent (host: Host, onlyAnon: boolean) {
  if (onlyAnon) {
    return formatLocaleTemplate(
      host.getString('notEnoughTokens'),
      { currency: host.getString('points') }
    )
  }
  const text = formatLocaleTemplate(
    host.getString('notEnoughTokensLink'),
    { currency: host.getString('tokens') }
  )
  return (
    <>
      {text}&nbsp;
      <NewTabLink href='brave://rewards/#add-funds'>
        {host.getString('addFunds')}
      </NewTabLink>.
    </>
  )
}

export function OneTimeTipForm () {
  const host = React.useContext(HostContext)
  const { getString } = host

  const [balanceInfo, setBalanceInfo] = React.useState(host.state.balanceInfo)
  const [rewardsParameters, setRewardsParameters] = React.useState(host.state.rewardsParameters)
  const [publisherInfo, setPublisherInfo] = React.useState(host.state.publisherInfo)
  const [onlyAnon, setOnlyAnon] = React.useState(Boolean(host.state.onlyAnonWallet))

  const [paymentKind, setPaymentKind] = React.useState<PaymentKind>('bat')
  const [tipAmount, setTipAmount] = React.useState<number>(0)

  React.useEffect(() => {
    return host.addListener((state) => {
      setBalanceInfo(state.balanceInfo)
      setRewardsParameters(state.rewardsParameters)
      setPublisherInfo(state.publisherInfo)
      setOnlyAnon(Boolean(state.onlyAnonWallet))
    })
  }, [host])

  React.useEffect(() => {
    // Select a default tip amount
    if (tipAmount === 0 && balanceInfo && rewardsParameters && publisherInfo) {
      const options = generateTipOptions(rewardsParameters, publisherInfo)
      setTipAmount(getDefaultTipOption(options, balanceInfo.total))
    }
  })

  if (!balanceInfo || !rewardsParameters || !publisherInfo) {
    return null
  }

  function processTip () {
    if (tipAmount > 0) {
      host.processTip(tipAmount, 'one-time')
    }
  }

  const tipOptions = generateTipOptions(rewardsParameters, publisherInfo)

  const tipAmountOptions = tipOptions.map((value) => ({
    value,
    amount: value.toFixed(0),
    currency: <BatString />,
    exchangeAmount: formatExchangeAmount(value, rewardsParameters.rate)
  }))

  return (
    <style.root>
      <style.main>
        <PaymentKindSwitch
          userBalance={balanceInfo.total}
          currentValue={paymentKind}
          onChange={setPaymentKind}
        />
        <style.amounts>
          <TipAmountSelector
            options={tipAmountOptions}
            selectedValue={tipAmount}
            onSelect={setTipAmount}
          />
        </style.amounts>
      </style.main>
      <style.footer>
        <TermsOfService />
        {
          balanceInfo.total < tipAmount ?
            <style.addFunds>
              <style.sadIcon>
                <EmoteSadIcon />
              </style.sadIcon> {getInsufficientFundsContent(host, onlyAnon)}
            </style.addFunds> :
            <FormSubmitButton onClick={processTip}>
              <PaperAirplaneIcon /> {getString('sendDonation')}
            </FormSubmitButton>
        }
      </style.footer>
    </style.root>
  )
}
