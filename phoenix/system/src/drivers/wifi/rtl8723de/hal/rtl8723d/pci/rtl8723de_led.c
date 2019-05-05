/******************************************************************************
 *
 * Copyright(c) 2007 - 2017 Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 *****************************************************************************/

#include "drv_types.h"
#include <rtl8723d_hal.h>
#ifdef CONFIG_RTW_SW_LED

/* ================================================================================
 * Prototype of protected function.
 * ================================================================================ */

/* ================================================================================
 * LED_819xUsb routines.
 * ================================================================================ */

/*
 *	Description:
 *		Turn on LED according to LedPin specified.
 */
void
SwLedOn_8723DE(
		PADAPTER		Adapter,
		PLED_PCIE		pLed
)
{
	u16	LedReg = REG_LEDCFG0;
	u8	LedCfg = 0;
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(Adapter);
	struct led_priv	*ledpriv = adapter_to_led(Adapter);

	if (RTW_CANNOT_RUN(Adapter))
		return;

	pLed->bLedOn = _TRUE;
}


/*
 *	Description:
 *		Turn off LED according to LedPin specified.
 */
void
SwLedOff_8723DE(
		PADAPTER		Adapter,
		PLED_PCIE		pLed
)
{
	u16	LedReg = REG_LEDCFG0;
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(Adapter);
	struct led_priv	*ledpriv = adapter_to_led(Adapter);

	if (RTW_CANNOT_RUN(Adapter))
		return;

	pLed->bLedOn = _FALSE;
}


/*
 *	Description:
 *		Initialize all LED_871x objects.
 */
void
rtl8723de_InitSwLeds(
	_adapter	*padapter
)
{
	struct led_priv *pledpriv = adapter_to_led(padapter);

	pledpriv->LedControlHandler = LedControlPCIE;

	pledpriv->SwLedOn = SwLedOn_8723DE;
	pledpriv->SwLedOff = SwLedOff_8723DE;

	InitLed(padapter, &(pledpriv->SwLed0), LED_PIN_LED0);

	InitLed(padapter, &(pledpriv->SwLed1), LED_PIN_LED1);
}


/*
 *	Description:
 *		DeInitialize all LED_819xUsb objects.
 */
void
rtl8723de_DeInitSwLeds(
	_adapter	*padapter
)
{
	struct led_priv	*ledpriv = adapter_to_led(padapter);

	DeInitLed(&(ledpriv->SwLed0));
	DeInitLed(&(ledpriv->SwLed1));
}
#endif
