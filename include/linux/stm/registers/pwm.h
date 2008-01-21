#ifndef __STM_REGISTERS_PWM_H
#define __STM_REGISTERS_PWM_H



#define PWM_VAL(n) (0x00 + n * 0x04) /* R/W */

#define PWM_VAL__PWM_VAL__SHIFT 0
#define PWM_VAL__PWM_VAL__MASK  0xff



#define PWM_CPT_VAL(n) (0x10 + (n * 0x04)) /* RO */

#define PWM_CPT_VAL__CPT_VAL__SHIFT 0
#define PWM_CPT_VAL__CPT_VAL__MASK  0xffffffff



#define PWM_CMP_VAL(n) (0x20 + (n * 0x04)) /* R/W */

#define PWM_CMP_VAL__CMP_VAL__SHIFT 0
#define PWM_CMP_VAL__CMP_VAL__MASK  0xffffffff



#define PWM_CPT_EDGE(n) (0x30 + (n * 0x04)) /* R/W */

#define PWM_CPT_EDGE__CE__SHIFT 0
#define PWM_CPT_EDGE__CE__MASK  0x3



#define PWM_CMP_OUT_VAL(n) (0x40 + (n * 0x04)) /* R/W */

#define PWM_CMP_OUT_VAL__CO__SHIFT 0
#define PWM_CMP_OUT_VAL__CO__MASK  0x1



#define PWM_CTRL 0x50 /* R/W */

#define PWM_CTRL__PWM_CLK_VAL_3_0__SHIFT 0
#define PWM_CTRL__PWM_CLK_VAL_3_0__MASK  0xf

#define PWM_CTRL__CPT_CLK_VAL_4_0__SHIFT 4
#define PWM_CTRL__CPT_CLK_VAL_4_0__MASK  0x1f

#define PWM_CTRL__PWM_EN__SHIFT 9
#define PWM_CTRL__PWM_EN__MASK  0x1

#define PWM_CTRL__CPT_EN__SHIFT 10
#define PWM_CTRL__CPT_EN__MASK  0x1

#define PWM_CTRL__PWM_CLK_VAL_7_4__SHIFT 11
#define PWM_CTRL__PWM_CLK_VAL_7_4__MASK	 0xf



#define PWM_INT_EN 0x54 /* R/W */

#define PWM_INT_EN__EN__SHIFT 0
#define PWM_INT_EN__EN__MASK  0x1

#define PWM_INT_EN__CPT0_INT_EN__SHIFT 1
#define PWM_INT_EN__CPT0_INT_EN__MASK  0x1

#define PWM_INT_EN__CPT1_INT_EN__SHIFT 2
#define PWM_INT_EN__CPT1_INT_EN__MASK  0x1

#define PWM_INT_EN__CMP0_INT_EN__SHIFT 5
#define PWM_INT_EN__CMP0_INT_EN__MASK  0x1

#define PWM_INT_EN__CMP1_INT_EN__SHIFT 6
#define PWM_INT_EN__CMP1_INT_EN__MASK  0x1



#define PWM_INT_STA 0x58 /* RO */

#define PWM_INT_STA__PWM_INT__SHIFT  0
#define PWM_INT_STA__PWM_INT__MASK   0x1

#define PWM_INT_STA__CPT0_INT__SHIFT 1
#define PWM_INT_STA__CPT0_INT__MASK  0x1

#define PWM_INT_STA__CPT1_INT__SHIFT 2
#define PWM_INT_STA__CPT1_INT__MASK  0x1

#define PWM_INT_STA__CMP0_INT__SHIFT 5
#define PWM_INT_STA__CMP0_INT__MASK  0x1

#define PWM_INT_STA__CMP1_INT__SHIFT 6
#define PWM_INT_STA__CMP1_INT__MASK  0x1



#define PWM_INT_ACK 0x5c /* WO */

#define PWM_INT_ACK__PWM_INT__SHIFT 0
#define PWM_INT_ACK__PWM_INT__MASK  0x1

#define PWM_INT_ACK__CPT0_INT__SHIFT 1
#define PWM_INT_ACK__CPT0_INT__MASK  0x1

#define PWM_INT_ACK__CPT1_INT__SHIFT 2
#define PWM_INT_ACK__CPT1_INT__MASK  0x1

#define PWM_INT_ACK__CMP0_INT__SHIFT 5
#define PWM_INT_ACK__CMP0_INT__MASK  0x1

#define PWM_INT_ACK__CMP1_INT__SHIFT 6
#define PWM_INT_ACK__CMP1_INT__MASK  0x1



#define PWM_CNT 0x60 /* R, W only when PWM timer is disabled */

#define PWM_CNT__PWM_CNT__SHIFT 0
#define PWM_CNT__PWM_CNT__MASK  0xff



#define PWM_CPT_CMP_CNT 0x64 /* R/W */

#define PWM_CPT_CMP_CNT__CPT_CMP_CNT__SHIFT 0
#define PWM_CPT_CMP_CNT__CPT_CMP_CNT__MASK  0xffffffff



#endif
