/* linux/drivers/media/video/samsung/ipc_table.h
 *
 * Header file for Samsung IPC driver
 *
 * Jonghun Han, Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
  * Youngmok Song, Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __IPCTABLE_H__
#define __IPCTABLE_H__

/*  Horizontal Y 8tap  */
const s8 ipc_8tap_coef_y_h[] =
{
	/* IPC_PP_H_NORMAL */
	0,	0,	 0,	 0,	127,	0,	0,	0,
	0,	1,	-2,	 8,	126,	-6,	2,	-1,
	0,	1,	-5,	16,	125,	-12,	4,	-1,
	0,	2,	-8,	25,	121,	-16,	5,	-1,
	-1,	3,	-10,	35,	114,	-18,	6,	-1,
	-1,	4,	-13,	46,	107,	-20,	6,	-1,
	-1,	5,	-16,	57,	99,	-21,	6,	-1,
	-1,	5,	-18,	68,	89,	-20,	6,	-1,
	-1,	6,	-20,	79,	79,	-20,	6,	-1,
	-1,	6,	-20,	89,	68,	-18,	5,	-1,
	-1,	6,	-21,	99,	57,	-16,	5,	-1,
	-1,	6,	-20,	107,	46,	-13,	4,	-1,
	-1,	6,	-18,	114,	35,	-10,	3,	-1,
	-1,	5,	-16,	121,	25,	-8,	2,	0,
	-1,	4,	-12,	125,	16,	-5,	1,	0,
	-1,	2,	-6,	126,	8,	-2,	1,	0,

	/* IPC_PP_H_8_9 */
	0,	3,	-7,	12,	112,	12,	-7,	3,
	-1,	3,	-9,	19,	113,	6,	-5,	2,
	-1,	3,	-11,	27,	111,	0,	-3,	2,
	-1,	4,	-13,	35,	108,	-5,	-1,	1,
	-1,	4,	-14,	43,	104,	-9,	0,	1,
	-1,	5,	-16,	52,	99,	-12,	1,	0,
	-1,	5,	-17,	61,	92,	-14,	2,	0,
	0,	4,	-17,	69,	85,	-16,	3,	0,
	0,	4,	-17,	77,	77,	-17,	4,	0,
	0,	3,	-16,	85,	69,	-17,	4,	0,
	0,	2,	-14,	92,	61,	-17,	5,	-1,
	0,	1,	-12,	99,	52,	-16,	5,	-1,
	1,	0,	-9,	104,	43,	-14,	4,	-1,
	1,	-1,	-5,	108,	35,	-13,	4,	-1,
	2,	-3,	0,	111,	27,	-11,	3,	-1,
	2,	-5,	6,	113,	19,	-9,	3,	-1,

	/* IPC_PP_H_1_2 */				  
	0,	-3,	0,	35,	64,	35,	0,	-3,
	0,	-3,	1,	38,	64,	32,	-1,	-3,
	0,	-3,	2,	41,	63,	29,	-2,	-2,
	0,	-4,	4,	43,	63,	27,	-3,	-2,
	0,	-4,	5,	46,	62,	24,	-3,	-2,
	0,	-4,	7,	49,	60,	21,	-3,	-2,
	-1,	-4,	9,	51,	59,	19,	-4,	-1,
	-1,	-4,	12,	53,	57,	16,	-4,	-1,
	-1,	-4,	14,	55,	55,	14,	-4,	-1,
	-1,	-4,	16,	57,	53,	12,	-4,	-1,
	-1,	-4,	19,	59,	51,	9,	-4,	-1,
	-2,	-3,	21,	60,	49,	7,	-4,	0,
	-2,	-3,	24,	62,	46,	5,	-4,	0,
	-2,	-3,	27,	63,	43,	4,	-4,	0,
	-2,	-2,	29,	63,	41,	2,	-3,	0,
	-3,	-1,	32,	64,	38,	1,	-3,	0,

	/* IPC_PP_H_1_3 */			 
	0,	0,	10,	32,	44,	32,	10,	0,
	-1,	0,	11,	33,	45,	31,	9,	0,
	-1,	0,	12,	35,	45,	29,	8,	0,
	-1,	1,	13,	36,	44,	28,	7,	0,
	-1,	1,	15,	37,	44,	26,	6,	0,
	-1,	2,	16,	38,	43,	25,	5,	0,
	-1,	2,	18,	39,	43,	23,	5,	-1,
	-1,	3,	19,	40,	42,	22,	4,	-1,
	-1,	3,	21,	41,	41,	21,	3,	-1,
	-1,	4,	22,	42,	40,	19,	3,	-1,
	-1,	5,	23,	43,	39,	18,	2,	-1,
	0,	5,	25,	43,	38,	16,	2,	-1,
	0,	6,	26,	44,	37,	15,	1,	-1,
	0,	7,	28,	44,	36,	13,	1,	-1,
	0,	8,	29,	45,	35,	12,	0,	-1,
	0,	9,	31,	45,	33,	11,	0,	-1,

	/* IPC_PP_H_1_4 */
	0,	2,	13,	30,	38,	30,	13,	2,
	0,	3,	14,	30,	38,	29,	12,	2,
	0,	3,	15,	31,	38,	28,	11,	2,
	0,	4,	16,	32,	38,	27,	10,	1,
	0,	4,	17,	33,	37,	26,	10,	1,
	0,	5,	18,	34,	37,	24,	9,	1,
	0,	5,	19,	34,	37,	24,	8,	1,
	1,	6,	20,	35,	36,	22,	7,	1,
	1,	6,	21,	36,	36,	21,	6,	1,
	1,	7,	22,	36,	35,	20,	6,	1,
	1,	8,	24,	37,	34,	19,	5,	0,
	1,	9,	24,	37,	34,	18,	5,	0,
	1,	10,	26,	37,	33,	17,	4,	0,
	1,	10,	27,	38,	32,	16,	4,	0,
	2,	11,	28,	38,	31,	15,	3,	0,
	2,	12,	29,	38,	30,	14,	3,	0
};

/* Horizontal C 4tap */
const s8 ipc_4tap_coef_c_h[] =
{
	/* IPC_PP_H_NORMAL */
	0,	0,	127,	0,
	0,	5,	126,	-3,
	-1,	11,	124,	-6,
	-1,	19,	118,	-8,
	-2,	27,	111,	-8,
	-3,	37,	102,	-8,
	-4,	48,	92,	-8,
	-5,	59,	81,	-7,
	-6,	70,	70,	-6,
	-7,	81,	59,	-5,
	-8,	92,	48,	-4,
	-8,	102,	37,	-3,
	-8,	111,	27,	-2,
	-8,	118,	19,	-1,
	-6,	124,	11,	-1,
	-3,	126,	5,	0,

	/* IPC_PP_H_8_9 */
	0,	 8,	112,	8,
	-1,	13,	113,	3,
	-2,	19,	111,	0,
	-2,	26,	107,	-3,
	-3,	34,	101,	-4,
	-3,	42,	94,	-5,
	-4,	51,	86,	-5,
	-5,	60,	78,	-5,
	-5,	69,	69,	-5,
	-5,	78,	60,	-5,
	-5,	86,	51,	-4,
	-5,	94,	42,	-3,
	-4,	101,	34,	-3,
	-3,	107,	26,	-2,
	0,	111,	19,	-2,
	3,	113,	13,	-1,

	/* 	IPC_PP_H_1_2 */	 
	0,	26,	76,	26,
	0,	30,	76,	22,
	0,	34,	75,	19,
	1,	38,	73,	16,
	1,	43,	71,	13,
	2,	47,	69,	10,
	3,	51,	66,	8,
	4,	55,	63,	6,
	5,	59,	59,	5,
	6,	63,	55,	4,
	8,	66,	51,	3,
	10,	69,	47,	2,
	13,	71,	43,	1,
	16,	73,	38,	1,
	19,	75,	34,	0,
	22,	76,	30,	0,

	/*	IPC_PP_H_1_3 */
	0,	30,	68,	30,
	2,	33,	66,	27,
	3,	36,	66,	23,
	3,	39,	65,	21,
	4,	43,	63,	18,
	5,	46,	62,	15,
	6,	49,	60,	13,
	8,	52,	57,	11,
	9,	55,	55,	9,
	11,	57,	52,	8,
	13,	60,	49,	6,
	15,	62,	46,	5,
	18,	63,	43,	4,
	21,	65,	39,	3,
	23,	66,	36,	3,
	27,	66,	33,	2,

	/*  IPC_PP_H_1_4 */
	0,	31,	66,	31,
	3,	34,	63,	28,
	4,	37,	62,	25,
	4,	40,	62,	22,
	5,	43,	61,	19,
	6,	46,	59,	17,
	7,	48,	58,	15,
	9,	51,	55,	13,
	11,	53,	53,	11,
	13,	55,	51,	9,
	15,	58,	48,	7,
	17,	59,	46,	6,
	19,	61,	43,	5,
	22,	62,	40,	4,
	25,	62,	37,	4,
	28,	63,	34,	3,
};


/*  Vertical Y 8tap  */
const s8 ipc_4tap_coef_y_v[] =
{
	/* IPC_PP_V_NORMAL  */	
	0,	0,	127,	0,
	0,	5,	126,	-3,
	-1,	11,	124,	-6,
	-1,	19,	118,	-8,
	-2,	27,	111,	-8,
	-3,	37,	102,	-8,
	-4,	48,	92,	-8,
	-5,	59,	81,	-7,
	-6,	70,	70,	-6,
	-7,	81,	59,	-5,
	-8,	92,	48,	-4,
	-8,	102,	37,	-3,
	-8,	111,	27,	-2,
	-8,	118,	19,	-1,
	-6,	124,	11,	-1,
	-3,	126,	5,	0,

	/* IPC_PP_V_5_6  */	
	0,	11,	106,	11,
	-2,	16,	107,	7,
	-2,	22,	105,	3,
	-2,	29,	101,	0,
	-3,	36,	96,	-1,
	-3,	44,	90,	-3,
	-4,	52,	84,	-4,
	-4,	60,	76,	-4,
	-4,	68,	68,	-4,
	-4,	76,	60,	-4,
	-4,	84,	52,	-4,
	-3,	90,	44,	-3,
	-1,	96,	36,	-3,
	0,	101,	29,	-2,
	3,	105,	22,	-2,
	7,	107,	16,	-2,

	/* IPC_PP_V_3_4  */	
	0,	15,	98,	15,
	-2,	21,	97,	12,
	-2,	26,	96,	8,
	-2,	32,	93,	5,
	-2,	39,	89,	2,
	-2,	46,	84,	0,
	-3,	53,	79,	-1,
	-2,	59,	73,	-2,
	-2,	66,	66,	-2,
	-2,	73,	59,	-2,
	-1,	79,	53,	-3,
	0,	84,	46,	-2,
	2,	89,	39,	-2,
	5,	93,	32,	-2,
	8,	96,	26,	-2,
	12,	97,	21,	-2,

	/* IPC_PP_V_1_2  */	
	0,	26,	76,	26,
	0,	30,	76,	22,
	0,	34,	75,	19,
	1,	38,	73,	16,
	1,	43,	71,	13,
	2,	47,	69,	10,
	3,	51,	66,	8,
	4,	55,	63,	6,
	5,	59,	59,	5,
	6,	63,	55,	4,
	8,	66,	51,	3,
	10,	69,	47,	2,
	13,	71,	43,	1,
	16,	73,	38,	1,
	19,	75,	34,	0,
	22,	76,	30,	0,

	/* IPC_PP_V_1_3  */	
	0,	30,	68,	30,
	2,	33,	66,	27,
	3,	36,	66,	23,
	3,	39,	65,	21,
	4,	43,	63,	18,
	5,	46,	62,	15,
	6,	49,	60,	13,
	8,	52,	57,	11,
	9,	55,	55,	9,
	11,	57,	52,	8,
	13,	60,	49,	6,
	15,	62,	46,	5,
	18,	63,	43,	4,
	21,	65,	39,	3,
	23,	66,	36,	3,
	27,	66,	33,	2,

	/* IPC_PP_V_1_4  */	
	0,	31,	66,	31,
	3,	34,	63,	28,
	4,	37,	62,	25,
	4,	40,	62,	22,
	5,	43,	61,	19,
	6,	46,	59,	17,
	7,	48,	58,	15,
	9,	51,	55,	13,
	11,	53,	53,	11,
	13,	55,	51,	9,
	15,	58,	48,	7,
	17,	59,	46,	6,
	19,	61,	43,	5,
	22,	62,	40,	4,
	25,	62,	37,	4,
	28,	63,	34,	3
};
#endif
