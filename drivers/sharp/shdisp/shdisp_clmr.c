/* drivers/sharp/shdisp/shdisp_clmr.c  (Display Driver)
 *
 * Copyright (C) 2012-2013 SHARP CORPORATION
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*---------------------------------------------------------------------------*/
/* SHARP DISPLAY DRIVER FOR KERNEL MODE                                      */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/* INCLUDE FILES                                                             */
/*---------------------------------------------------------------------------*/

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/completion.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#ifdef CONFIG_TOUCHSCREEN_SHTPS
#include <sharp/shtps_dev.h>
#endif /* CONFIG_TOUCHSCREEN_SHTPS */
#include <sharp/sh_boot_manager.h>
#ifdef KERNEL_CALL_PIC_ADJ_MDP
#include <linux/msm_mdp.h>
#endif

#include "../../../arch/arm/mach-msm/board-8064.h"

#include "shdisp_bdic.h"
#include "shdisp_system.h"
#include "shdisp_clmr.h"
#include "shdisp_clmr_fw.h"
#include "shdisp_dbg.h"
#include "shdisp_pm.h"
#include "shdisp_panel.h"
#if defined(CONFIG_MACH_LYNX_DL50)
  #include "data/shdisp_pic_adj_data_dl50.h"
  #include "data/shdisp_trv_data_dl50.h"
  #include "data/shdisp_ae_data_dl50.h"
  #include "data/shdisp_smite_data_dl50.h"
#elif defined(CONFIG_MACH_EBZ)
  #include "data/shdisp_pic_adj_data_pa24.h"
  #include "data/shdisp_trv_data_pa24.h"
  #include "data/shdisp_ae_data_pa24.h"
  #include "data/shdisp_smite_data_pa24.h"
#elif  defined(CONFIG_MACH_DECKARD_AS99)
  #include "data/shdisp_pic_adj_data_as99.h"
  #include "data/shdisp_trv_data_as99.h"
  #include "data/shdisp_ae_data_as99.h"
  #include "data/shdisp_smite_data_as99.h"
#elif  defined(CONFIG_MACH_LYNX_GP9D)
  #include "data/shdisp_pic_adj_data_gp9d.h"
  #include "data/shdisp_trv_data_gp9d.h"
  #include "data/shdisp_ae_data_gp9d.h"
  #include "data/shdisp_smite_data_gp9d.h"
#else /* CONFIG_MACH_DEFAULT */
  #include "data/shdisp_pic_adj_data_default.h"
  #include "data/shdisp_trv_data_default.h"
  #include "data/shdisp_ae_data_default.h"
  #include "data/shdisp_smite_data_default.h"
#endif /* CONFIG_MACH_ */



/*---------------------------------------------------------------------------*/
/* MACROS                                                                    */
/*---------------------------------------------------------------------------*/
#define  CALI_HOSTBASE_VAL 0x00007CE0

#define SHDISP_CLMR_ENABLE      1
#define SHDISP_CLMR_DISABLE     0


#define SHDISP_CLMR_SET_DSCTL           0x00000080

#define SHDISP_CLMR_WAIT4SYNC_TIMEOUT   (msecs_to_jiffies(2100))

#define SHDISP_CLMR_DEVCODE_VALUE               0x00004002

#define SHDISP_CLMR_FWCMD_HOST_EWB_LUT_WRITE_SIZE       1026
#define SHDISP_CLMR_FWCMD_LUT_ON_SIZE                   2
#define SHDISP_CLMR_FWCMD_LUT_ON_CPF                    0
#define SHDISP_CLMR_FWCMD_LUT_ON_EWB                    1
#define SHDISP_CLMR_FWCMD_LUT_ON_TRV                    2

#define SHDISP_CLMR_FWCMD_HOST_WORD_WRITE_SIZE          6
#define SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE_SIZE    10
#define SHDISP_CLMR_EWB_OFF                             0
#define SHDISP_CLMR_EWB_ON                              1
#define SHDISP_CLMR_TRV_OFF                             0
#define SHDISP_CLMR_TRV_ON                              1
#define SHDISP_CLMR_FWCMD_HOST_TRV_LUT_WRITE_SIZE       1026
#define SHDISP_CLMR_FWCMD_TRV_MIF_SIZE                  6
#if defined(CONFIG_SHDISP_PANEL_GEMINI)
  #define SHDISP_CLMR_TRV_BASE                          0x0000788A
#elif defined(CONFIG_SHDISP_PANEL_RYOMA)
  #define SHDISP_CLMR_TRV_BASE                          0x00006081
#else
  #define SHDISP_CLMR_TRV_BASE                          0x0000762A
#endif
#define SHDISP_CLMR_FWCMD_HOST_VSPREGON_SIZE            2
#define SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_SIZE        2
#define SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_OFF         0
#define SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_CPF         1
#define SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_SBL         2
#define SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_TRV         4
#define SHDISP_CLMR_SBL_OFF                             0
#define SHDISP_CLMR_SBL_AE                              1
#define SHDISP_CLMR_SBL_ACC                             2
#define SHDISP_CLMR_SBL_NO_CHG                          3
#define SHDISP_CLMR_FWCMD_LIGHTCTL_LUXMODE_SET_OFF      0x00
#define SHDISP_CLMR_FWCMD_LIGHTCTL_LUXMODE_SET_SBL      0x04
#define SHDISP_CLMR_FWCMD_LIGHTCTL_LUXMODE_SET_SMITE    0x02

#define SHDISP_CLMR_FWCMD_HOST_CPF1_LUT_WRITE_SIZE      514
#define SHDISP_CLMR_FWCMD_HOST_CPF2_LUT_WRITE_SIZE      1026
#define SHDISP_CLMR_SMITE_OFF                           0
#define SHDISP_CLMR_SMITE_ON                            1
#define SHDISP_CLMR_SMITE_MODE_CHG                      2
#define SHDISP_CLMR_FWCMD_SMITE_WRITE_CONFIG_SIZE_128   130
#define SHDISP_CLMR_FWCMD_SMITE_WRITE_CONFIG_SIZE_64    66
#define SHDISP_CLMR_FWCMD_SMITE_WRITE_CONFIG_SIZE_32    34
#define SHDISP_CLMR_FWCMD_SMITE_SET_MODE_SIZE           2
#define SHDISP_CLMR_FWCMD_SMITE_COMMIT_SIZE             0
#if defined(CONFIG_SHDISP_PANEL_ANDY) || defined(CONFIG_SHDISP_PANEL_RYOMA) || defined(CONFIG_SHDISP_PANEL_GEMINI)
#define SHDISP_CLMR_FWCMD_SMITE_SET_MODE_OFF            0x0000
#define SHDISP_CLMR_FWCMD_SMITE_SET_MODE_LPMC           0x0028
#define SHDISP_CLMR_FWCMD_SMITE_SET_MODE_DBC_ACC        0x00C9
#define SHDISP_CLMR_FWCMD_SMITE_SET_MODE_DBC            0x00A9
#define SHDISP_CLMR_FWCMD_SMITE_SET_MODE_ACC            0x0048
#else
#define SHDISP_CLMR_FWCMD_SMITE_SET_MODE_OFF            0x0000
#define SHDISP_CLMR_FWCMD_SMITE_SET_MODE_LPMC           0x0028
#define SHDISP_CLMR_FWCMD_SMITE_SET_MODE_DBC_ACC        0x00C1
#define SHDISP_CLMR_FWCMD_SMITE_SET_MODE_DBC            0x00A1
#define SHDISP_CLMR_FWCMD_SMITE_SET_MODE_ACC            0x0040
#endif
#define SHDISP_CLMR_FWCMD_LIGHTCTL_LUXMODE_SET_SIZE     1
#define SHDISP_CLMR_FWCMD_AE_TIME_SET_SIZE              1
#define SHDISP_CLMR_FWCMD_AE_MODE_SIZE                  1
#define SHDISP_CLMR_AE_OFF                              0
#define SHDISP_CLMR_AE_ON                               1
#define SHDISP_CLMR_AE_NO_CHG                           3
#define SHDISP_CLMR_AE_OFF_AE_ONLY                      0
#define SHDISP_CLMR_AE_OFF_WITH_HSV_PCA                 1
#define SHDISP_CLMR_FWCMD_HOST_MULTI_WRITE_ONE_SIZE     6
#define SHDISP_CLMR_FWCMD_HOST_MULTI_WRITE_MAX_SIZE     64

enum {
    TRV_MATRIX_OFF,
    TRV_MATRIX_ON,
    NUM_TRV_MATRIX
};
enum {
    DBC_ACC_MATRIX_OFF,
    DBC_ACC_MATRIX_DBC,
    DBC_ACC_MATRIX_ACC,
    DBC_ACC_MATRIX_BOTH,
    NUM_DBC_ACC_MATRIX
};
enum {
    PIC_ADJ_MATRIX_SVCT,
    PIC_ADJ_MATRIX_HSV,
    PIC_ADJ_MATRIX_PCA,
    PIC_ADJ_MATRIX_CPF,
    PIC_ADJ_MATRIX_AE,
    PIC_ADJ_MATRIX_SBL,
    PIC_ADJ_MATRIX_SMITE,
    PIC_ADJ_MATRIX_TRV,
    NUM_PIC_ADJ_MATRIX
};

#ifdef SHDISP_CLMR_FW_TIMEOUT_DUMP
static void shdisp_clmr_ram_dump_begin(void);
static size_t shdisp_clmr_edram_dump(unsigned char *edram_dump_buf, size_t length);
static size_t shdisp_clmr_sram_dump(unsigned char *sram_dump_buf, size_t length);
static void shdisp_clmr_ram_dump_end(void);
#endif /* SHDISP_CLMR_FW_TIMEOUT_DUMP */

#define SHDISP_CLMR_PWRON_RESULT_SUCCESS        0x00
#define SHDISP_CLMR_PWRON_RESULT_REGULATOR_ERR  0x01
#define SHDISP_CLMR_PWRON_RESULT_GPIO_ERR       0x02
#define SHDISP_CLMR_PWRON_RESULT_DEVCHK_ERR     0x03
#define SHDISP_CLMR_PWRON_RESULT_PLL_ERR        0x04
#define SHDISP_CLMR_PWRON_RESULT_BOOTFW_ERR     0x05

/*---------------------------------------------------------------------------*/
/* VARIABLES                                                                 */
/*---------------------------------------------------------------------------*/
static int shdisp_clmr_irq = 0;
static int gpclk_init = 0;


enum clmr_vreg_type {
    REG_LDO,
    REG_VS,
    REG_GPIO,
};

typedef struct {
    const char *reg_name;
    enum clmr_vreg_type type;
    int min_voltage;
    int max_voltage;
    int op_mode;
    uint32_t delay;     /* usec */
} clmr_vreg_t;

static clmr_vreg_t apq8064_clmr_vreg[] = {

    {"clmr_vdd_18tx",   REG_VS,        0,       0,      0, 1 * 1000},
    {"clmr_vdd_12lp",   REG_LDO, 1200000, 1200000, 100000, 1 * 1000},
};

static struct shdisp_clmr_ctrl_t {
    struct completion               fw_boot_comp;
    struct completion               fw_cmd_comp;
    struct completion               eDramPtr_rst_comp;
    struct semaphore                fw_boot_sem;
    struct semaphore                fw_cmd_sem;
    struct semaphore                eDramPtr_rst_sem;
    struct semaphore                wq_sem;
    unsigned char                   fw_boot_excute;
    unsigned char                   fw_cmd_excute;
    unsigned char                   eDramPtr_rst_excute;
    struct work_struct              work;
    struct workqueue_struct        *workqueue;
    spinlock_t                      spin_lock;
    struct wake_lock                wake_lock;
    clmr_vreg_t                     *clmr_vreg;
    struct regulator                **clmr_regu;
    struct platform_device          *pdev;
    int                               core_reg_gpio;
    unsigned int                   tear_int_count;
    unsigned long                  tear_log_disable_time;
#if defined(CONFIG_ANDROID_ENGINEERING)
    unsigned int                    pwroff_fwlog_dump_flg;
#endif /* defined (CONFIG_ANDROID_ENGINEERING) */
} shdisp_clmr_ctrl;

static struct shdisp_clmr_handshake_t {
    unsigned char                   cmdno;
    unsigned char                   *rbuf;
    int                             size;
    int                             err;
} shdisp_clmr_handshake;

static struct shdisp_clmr_ewb_accu clmr_ewb_accu[SHDISP_CLMR_EWB_LUT_NUM];
static unsigned char clmr_wdata[SHDISP_CLMR_FWCMD_HOST_EWB_LUT_WRITE_SIZE];
static struct shdisp_als_adjust clmr_als_adjust[2];
#if defined(CONFIG_SHDISP_PANEL_ANDY) || defined(CONFIG_SHDISP_PANEL_MARCO) || \
    defined(CONFIG_SHDISP_PANEL_CARIN) || defined(CONFIG_SHDISP_PANEL_GEMINI)
  #if defined(CONFIG_MACH_LYNX_DL50)
    #define LC_TH_L         (0x0100)
    #define LC_TH_H         (0x012F)
    #define LC_R_PWMT0      (0x0001)
    #define LC_R_PWMT1      (0x0000)
    #define LC_F_PWMT0      (0x0016)
    #define LC_F_PWMT1      (0x0001)
    #define LC_LTOH         (0x0230)
    #define LC_HTOL         (0x0155)
    #define LC_S0           (0x1000)
    #define LC_S1           (0x4000)
    #define LC_T0           (0x0000)
    #define LC_T1           (0x0000)
    #define LC_PWM_CYCLE    (0x0586)
    #define LC_MLED01       (0x0000)
    #define LC_MLEDL        (0x00BE)
    #define DBC_HL_TH       (0x0064)
    #define LC_LOWBL_TBL    (0x0001)
    #define SBL_BL_LIMIT    (0x0100)
    #define SBL_AL_LIMIT    (0x0489)
    #define LC_SENRNG_MODE  (0x0000)
  #elif defined(CONFIG_MACH_EBZ)
    #define LC_TH_L         (0x0100)
    #define LC_TH_H         (0x012F)
    #define LC_R_PWMT0      (0x0001)
    #define LC_R_PWMT1      (0x0000)
    #define LC_F_PWMT0      (0x0016)
    #define LC_F_PWMT1      (0x0001)
    #define LC_LTOH         (0x0156)
    #define LC_HTOL         (0x00D7)
    #define LC_S0           (0x1000)
    #define LC_S1           (0x4000)
    #define LC_T0           (0x0000)
    #define LC_T1           (0x0000)
    #define LC_PWM_CYCLE    (0x05EA)
    #define LC_MLED01       (0x0000)
    #define LC_MLEDL        (0x00BE)
    #define DBC_HL_TH       (0x0064)
    #define LC_LOWBL_TBL    (0x0001)
    #define SBL_BL_LIMIT    (0x0100)
    #define SBL_AL_LIMIT    (0x0432)
    #define LC_SENRNG_MODE  (0x0000)
  #elif defined(CONFIG_MACH_DECKARD_AS99)
    #define LC_TH_L         (0x0075)
    #define LC_TH_H         (0x0027)
    #define LC_R_PWMT0      (0x0001)
    #define LC_R_PWMT1      (0x0000)
    #define LC_F_PWMT0      (0x0063)
    #define LC_F_PWMT1      (0x0001)
    #define LC_LTOH         (0x08CB)
    #define LC_HTOL         (0x05D3)
    #define LC_S0           (0x1000)
    #define LC_S1           (0x4000)
    #define LC_T0           (0x0000)
    #define LC_T1           (0x0000)
    #define LC_PWM_CYCLE    (0x0698)
    #define LC_MLED01       (0x0000)
    #define LC_MLEDL        (0x00BE)
    #define DBC_HL_TH       (0x0064)
    #define LC_LOWBL_TBL    (0x0001)
    #define SBL_BL_LIMIT    (0x0180)
    #define SBL_AL_LIMIT    (0x03B7)
    #define LC_SENRNG_MODE  (0x0000)
  #elif defined(CONFIG_MACH_LYNX_GP9D)
    #define LC_TH_L         (0x005D)
    #define LC_TH_H         (0x008C)
    #define LC_R_PWMT0      (0x0001)
    #define LC_R_PWMT1      (0x0000)
    #define LC_F_PWMT0      (0x0063)
    #define LC_F_PWMT1      (0x0001)
    #define LC_LTOH         (0x0322)
    #define LC_HTOL         (0x0113)
    #define LC_S0           (0x1000)
    #define LC_S1           (0x4000)
    #define LC_T0           (0x0000)
    #define LC_T1           (0x0000)
    #define LC_PWM_CYCLE    (0x0695)
    #define LC_MLED01       (0x0000)
    #define LC_MLEDL        (0x00BE)
    #define DBC_HL_TH       (0x0064)
    #define LC_LOWBL_TBL    (0x0001)
    #define SBL_BL_LIMIT    (0x0100)
    #define SBL_AL_LIMIT    (0x0394)
    #define LC_SENRNG_MODE  (0x0000)
  #else
    #define LC_TH_L         (0x0075)
    #define LC_TH_H         (0x0027)
    #define LC_R_PWMT0      (0x0000)
    #define LC_R_PWMT1      (0x0000)
    #define LC_F_PWMT0      (0x0002)
    #define LC_F_PWMT1      (0x0001)
    #define LC_LTOH         (0x08CB)
    #define LC_HTOL         (0x05D3)
    #define LC_S0           (0x1000)
    #define LC_S1           (0x4000)
    #define LC_T0           (0x0000)
    #define LC_T1           (0x0000)
    #define LC_PWM_CYCLE    (0x0614)
    #define LC_MLED01       (0x0000)
    #define LC_MLEDL        (0x00BC)
    #define DBC_HL_TH       (0x0050)
    #define LC_LOWBL_TBL    (0x0001)
    #define SBL_BL_LIMIT    (0x0180)
    #define SBL_AL_LIMIT    (0x03B7)
    #define LC_SENRNG_MODE  (0x0000)
  #endif
static unsigned short shdisp_cal_fw_lc_parama[30] = {
    0x0000,
    0x0D0D,     LC_TH_L,   LC_TH_H,   0x0001,       0x0001,     LC_R_PWMT0,
    LC_R_PWMT1, 0x0001,    0x0001,    LC_F_PWMT0,   LC_F_PWMT1, 0x0000,
    0x0000,     0x0000,    0x0000,    0x0000,       0x0000,     0x0000,
    0x0000,     LC_LTOH,   LC_HTOL,   LC_S0,        LC_S1,      LC_T0 ,
    LC_T1,      0x0009,    0x0003,    0x0028,       LC_PWM_CYCLE
};

static unsigned short shdisp_cal_fw_lc_paramb[13] = {
    0xA200,
    LC_MLED01,      LC_MLEDL,        0x00A0,         DBC_HL_TH,
    0x003C,         0x0002,          0x0801,         0x0001,
    LC_LOWBL_TBL,   SBL_BL_LIMIT,    SBL_AL_LIMIT,   LC_SENRNG_MODE,
};

#elif defined(CONFIG_SHDISP_PANEL_RYOMA)
static unsigned short shdisp_cal_fw_lc_parama[30] = {
    0x0000,
    0x0D0D,    0x0027,    0x0027,    0x0001,    0x0001,    0x0001,
    0x0001,    0x0001,    0x0001,    0x0005,    0x0001,    0x0000,
    0x0000,    0x0000,    0x0000,    0x0000,    0x0000,    0x0000,
    0x0000,    0x0A5D,    0x04CF,    0x1000,    0x4000,    0x0000,
    0x0000,    0x0009,    0x0003,    0x0000,    0x068E
};
static unsigned short shdisp_cal_fw_lc_paramb[13] = {
    0xA200,
    0x0000,    0x006F,    0x00EF,    0x00A3,
    0x003C,    0x0063,    0x0801,    0x0064,
    0x0001,    0x01C0,    0x073F,    0x0000,
};
#endif

static unsigned short clmr_ap_type = SHDISP_LCDC_PIC_ADJ_AP_NORMAL;
static struct shdisp_main_pic_adj clmr_pic_adj = {0};
static struct shdisp_clmr_trv_info clmr_trv_info = {0};
static struct shdisp_main_dbc clmr_dbc = {0};
static struct shdisp_main_ae clmr_ae = {0};
static struct shdisp_clmr_ewb_accu clmr_ewb_accu_cross;

const static unsigned long cpf_mode_param[6] = {
    0x00001004, 0x00001004, 0x00001004, 0x00001004, 0x00001004, 0x00001004
};

static const unsigned short smite_matrix[NUM_DBC_ACC_MATRIX] =
{
#if defined(CONFIG_SHDISP_PANEL_ANDY) || defined(CONFIG_SHDISP_PANEL_RYOMA) || defined(CONFIG_SHDISP_PANEL_GEMINI)
    SHDISP_CLMR_FWCMD_SMITE_SET_MODE_LPMC,
    SHDISP_CLMR_FWCMD_SMITE_SET_MODE_DBC,
    SHDISP_CLMR_FWCMD_SMITE_SET_MODE_ACC,
    SHDISP_CLMR_FWCMD_SMITE_SET_MODE_DBC_ACC
#else
    SHDISP_CLMR_FWCMD_SMITE_SET_MODE_OFF,
    SHDISP_CLMR_FWCMD_SMITE_SET_MODE_DBC,
    SHDISP_CLMR_FWCMD_SMITE_SET_MODE_ACC,
    SHDISP_CLMR_FWCMD_SMITE_SET_MODE_DBC_ACC
#endif
};
static const unsigned char pic_adj_matrix[NUM_DBC_ACC_MATRIX][NUM_SHDISP_MAIN_DISP_PIC_ADJ_MODE][NUM_PIC_ADJ_MATRIX] =
{
    {
        {    0,     0,     0,     0,     0,     0,     0,     0},
        {    0,     0,     0,     0,     0,     0,     0,     0},
        {    1,     0,     0,     0,     0,     0,     0,     0},
        {    1,     0,     0,     0,     0,     0,     0,     0},
#if defined(CONFIG_MACH_LYNX_DL50)
        {    1,     0,     1,     1,     0,     0,     0,     0},
        {    1,     0,     1,     1,     0,     0,     0,     0},
#else /* CONFIG_MACH_LYNX_DL50 */
        {    0,     0,     1,     1,     0,     0,     0,     0},
        {    0,     0,     1,     1,     0,     0,     0,     0},
#endif /* CONFIG_MACH_LYNX_DL50 */
        {    1,     0,     1,     0,     1,     1,     0,     0}
    },
    {
        {    0,     0,     0,     0,     0,     0,     1,     0},
        {    0,     0,     0,     0,     0,     0,     1,     0},
        {    1,     0,     0,     0,     0,     0,     1,     0},
        {    1,     0,     0,     0,     0,     0,     1,     0},
#if defined(CONFIG_MACH_LYNX_DL50)
        {    1,     0,     1,     1,     0,     0,     1,     0},
        {    1,     0,     1,     1,     0,     0,     1,     0},
#else /* CONFIG_MACH_LYNX_DL50 */
        {    0,     0,     1,     1,     0,     0,     1,     0},
        {    0,     0,     1,     1,     0,     0,     1,     0},
#endif /* CONFIG_MACH_LYNX_DL50 */
        {    1,     0,     1,     0,     1,     1,     1,     0}
    },
    {
        {    0,     0,     0,     0,     0,     2,     2,     0},
        {    0,     0,     0,     0,     0,     2,     2,     0},
        {    1,     0,     0,     0,     0,     2,     2,     0},
        {    1,     0,     0,     0,     0,     2,     2,     0},
#if defined(CONFIG_MACH_LYNX_DL50)
        {    1,     0,     1,     1,     0,     2,     2,     0},
        {    1,     0,     1,     1,     0,     2,     2,     0},
#else /* CONFIG_MACH_LYNX_DL50 */
        {    0,     0,     1,     1,     0,     2,     2,     0},
        {    0,     0,     1,     1,     0,     2,     2,     0},
#endif /* CONFIG_MACH_LYNX_DL50 */
        {    1,     0,     1,     0,     1,     1,     2,     0}
    },
    {
        {    0,     0,     0,     0,     0,     2,     3,     0},
        {    0,     0,     0,     0,     0,     2,     3,     0},
        {    1,     0,     0,     0,     0,     2,     3,     0},
        {    1,     0,     0,     0,     0,     2,     3,     0},
#if defined(CONFIG_MACH_LYNX_DL50)
        {    1,     0,     1,     1,     0,     2,     3,     0},
        {    1,     0,     1,     1,     0,     2,     3,     0},
#else /* CONFIG_MACH_LYNX_DL50 */
        {    0,     0,     1,     1,     0,     2,     3,     0},
        {    0,     0,     1,     1,     0,     2,     3,     0},
#endif /* CONFIG_MACH_LYNX_DL50 */
        {    1,     0,     1,     0,     1,     1,     3,     0}
    }
};
static const unsigned char pic_adj_matrix_camera[NUM_DBC_ACC_MATRIX]
                                                [NUM_SHDISP_MAIN_DISP_PIC_ADJ_MODE]
                                                [NUM_PIC_ADJ_MATRIX] =
{
    {
        {    0,     0,     0,     0,     0,     0,     0,     0},
        {    1,     0,     0,     0,     0,     0,     0,     0},
    },
    {
        {    0,     0,     0,     0,     0,     0,     1,     0},
        {    1,     0,     0,     0,     0,     0,     1,     0},
    },
    {
        {    0,     0,     0,     0,     0,     2,     2,     0},
        {    1,     0,     0,     0,     0,     2,     2,     0}
    },
    {
        {    0,     0,     0,     0,     0,     2,     3,     0},
        {    1,     0,     0,     0,     0,     2,     3,     0}
    }
};
static const unsigned char pic_adj_matrix_trv[NUM_PIC_ADJ_MATRIX] =
{
             0,     0,     0,     0,     0,     0,     0,     1
};

#ifdef KERNEL_CALL_PIC_ADJ_MDP
static unsigned char hist_lut_data_pic_adj_mode_off_tbl[SHDISP_HIST_LUT_SIZE] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
#endif
static int clmr_need_dbc_startup = 1;

/*---------------------------------------------------------------------------*/
/* DEBUG MACROS                                                              */
/*---------------------------------------------------------------------------*/
static unsigned char *gArm_fw;
static unsigned long gArm_fw_size;
static unsigned short gArm_fw_base;
static unsigned char gArm_fw_chg_flg;

/*---------------------------------------------------------------------------*/
/* PROTOTYPES                                                                */
/*---------------------------------------------------------------------------*/
#ifdef KERNEL_CALL_PIC_ADJ_MDP
extern int msm_fb_set_pipe_hist_lut( struct fb_info *info, struct msmfb_pipe_hist_lut_data *hist_lut );
extern int msm_fb_set_pipe_qseed( struct fb_info *info, struct msmfb_pipe_qseed_data *qseed );
extern int msm_fb_set_ccs_matrix( struct fb_info *info, struct mdp_csc *csc_matrix );
#endif

static irqreturn_t shdisp_clmr_int_isr(int irq_num, void *data);
static void shdisp_workqueue_handler_clmr(struct work_struct *workq);

static int shdisp_clmr_lcdc_devcheck(void);
static int shdisp_clmr_request_irq(void);
static void shdisp_clmr_enable_irq(void);
static void shdisp_clmr_disable_irq(void);
static int shdisp_clmr_clock_setting(void);
static void shdisp_clmr_timing_setting(void);
static void shdisp_clmr_prepro_setting(void);
static void shdisp_clmr_mipi_dsi_rx_setting(void);
static void shdisp_clmr_mipi_dsi_tx_setting(void);
static void shdisp_clmr_mipi_dsi_tx_circuit_on(void);

#if defined(CONFIG_SHDISP_PANEL_GEMINI)
static void shdisp_clmr_tcon_setting(void);
#endif  /* CONFIG_SHDISP_PANEL_GEMINI */

static void shdisp_clmr_data_transfer_stops(void);
static void shdisp_clmr_mipi_dsi_tx_circuit_off(void);
static void shdisp_clmr_mipi_dsi_rx_circuit_off(void);

static int shdisp_clmr_gpio_reset_ctrl(int ctrl);
static int shdisp_clmr_regulator_init(void);
static int shdisp_clmr_regulator_on(void);
static int shdisp_clmr_regulator_off(void);
static int shdisp_clmr_gpio_request(void);
static int shdisp_clmr_gpio_free(void);
static int shdisp_clmr_boot_fw(void);
static int shdisp_clmr_api_fw_bdic_set_param(void);
static int shdisp_clmr_pd_fw_bdic_set_param(void);
static void shdisp_clmr_ld_set_sensor_param(unsigned short *sensor_lc_parama);

static void shdisp_clmr_arm_init(void);
static int shdisp_clmr_fw_download(void);
static int shdisp_clmr_arm_sram2fw(void);
static void shdisp_clmr_arm_boot(void);
static void shdisp_clmr_arm_reset(void);

static void shdisp_clmr_custom_blk_startup(void);
static void shdisp_clmr_custom_ewb_startup(void);
static void shdisp_clmr_custom_trv_startup(void);
static void shdisp_clmr_custom_dbc_startup(void);
static void shdisp_clmr_custom_pic_adj_startup(void);

static void shdisp_clmr_custom_blk_stop(void);

static int shdisp_clmr_wait4fw_boot_comp(void);

static int shdisp_clmr_regSet(const shdisp_clmrRegSetting_t *dat);

static int shdisp_clmr_regSet_multi(const shdisp_clmrRegSetting_t *regtable, int size);

static int shdisp_clmr_regSetwithFW(const shdisp_clmrRegSetting_t *dat);

static int shdisp_clmr_regsSetbyFW(const shdisp_clmrRegSetting_t *regtables, int size);
static int shdisp_clmr_or_readModifyWrite(
                        unsigned short reg, unsigned long lData);
static int shdisp_clmr_and_readModifyWrite(
                        unsigned short reg, unsigned long lMask);
static int shdisp_clmr_readModifyWrite(unsigned short reg,
                        unsigned long lData, unsigned long lMask);
static int shdisp_clmr_set_dsctl(void);

static int shdisp_clmr_config_reg(clmr_vreg_t *vreg, int num,
                                    struct regulator **reg_ptr, int config);
static int shdisp_clmr_enable_reg(clmr_vreg_t *vreg, int num,
                                    struct regulator **reg_ptr, int enable);

static void shdisp_clmr_donefw_boot_comp(void);
static void shdisp_clmr_donefw_cmd_comp(void);
static void shdisp_clmr_doneeDramPtr_rst_comp(void);
static int shdisp_clmr_reg_dump(unsigned short reg);
static int shdisp_clmr_hint_failsafe(struct completion *comp);

static void shdisp_clmr_boot_start(void);
static void shdisp_clmr_cmd_start(void);
static void shdisp_clmr_eDramPtr_rst_start(void);
#if defined(CONFIG_ANDROID_ENGINEERING)
static inline unsigned int shdisp_clmr_FWLogLength(void);
static unsigned int shdisp_clmr_FWLog_chronological(unsigned char *dst, unsigned char *src, unsigned int wp,
                                                                                              unsigned int rp );
static unsigned int shdisp_clmr_FWLog_get(unsigned char *buf, unsigned int len, int iseDramPtrRst);
static void shdisp_clmr_FWLogDump(unsigned char *buf, const unsigned int length);
#endif /* defined (CONFIG_ANDROID_ENGINEERING) */

static int shdisp_clmr_sqe_ewb_on(struct shdisp_clmr_ewb_accu *ewb_accu, unsigned char no);
static int shdisp_clmr_vsp_on(void);
static int shdisp_clmr_vsp_on_plus(unsigned char type);
static int shdisp_clmr_ewb_lut_write(struct shdisp_clmr_ewb_accu *ewb_accu, unsigned char no);
static int shdisp_clmr_ewb_param_set(void);
static int shdisp_clmr_ewb_on_off(int on);
static int shdisp_clmr_sqe_pic_adj_on(struct shdisp_main_pic_adj *pic_adj, unsigned short ap_type,
                                                                           const unsigned char *set);
static int shdisp_clmr_sqe_pic_adj_off(const unsigned char *off);
static int shdisp_clmr_sqe_trv_on(void);
static int shdisp_clmr_sqe_trv_off(void);
static int shdisp_clmr_sqe_trv_lut_chg(void);
static int shdisp_clmr_sqe_trv_img_chg(void);
static int shdisp_clmr_sqe_sbl_on(unsigned char mode, unsigned short ap_type);
static int shdisp_clmr_sqe_sbl_off(void);
static int shdisp_clmr_sqe_svct_on(unsigned short mode, unsigned short ap_type);
static int shdisp_clmr_sqe_svct_off(void);
static int shdisp_clmr_sqe_hsv_on(unsigned short mode, unsigned short ap_type);
static int shdisp_clmr_sqe_hsv_off(void);
static int shdisp_clmr_sqe_pca_on(unsigned short mode, unsigned short ap_type);
static int shdisp_clmr_sqe_pca_off(void);
static int shdisp_clmr_sqe_cpf_on(unsigned short mode, unsigned short ap_type);
static int shdisp_clmr_sqe_cpf_off(void);
int shdisp_clmr_sqe_cpf_mode_chg(unsigned short mode);
int shdisp_clmr_sqe_cpf_lut_chg(unsigned short mode, unsigned short ap_type);
static int shdisp_clmr_sqe_smite_on(unsigned short mode, unsigned short ap_type);
static int shdisp_clmr_sqe_smite_mode_chg(unsigned short mode);
static int shdisp_clmr_sqe_smite_off(void);
static int shdisp_clmr_sqe_smite_lpmc_setting_chg(unsigned short ap_type);
static int shdisp_clmr_sqe_ewb_lut_chg(void);
static int shdisp_clmr_sqe_ae_on(void);
static int shdisp_clmr_sqe_ae_off(unsigned char mode);
static int shdisp_clmr_sqe_ae_time_set(unsigned char time);
static int shdisp_clmr_trv_mif_set(unsigned short base, unsigned short hw, unsigned short y_size);
static int shdisp_clmr_trv_write_texture(void);
static int shdisp_clmr_trv_initial_setting(void);
static int shdisp_clmr_trv_lut_write(void);
static int shdisp_clmr_trv_param_set(void);
static int shdisp_clmr_trv_on(void);
static int shdisp_clmr_trv_off(void);
static int shdisp_clmr_trv_filter_off(void);
static int shdisp_clmr_trv_filter_on(void);
static int shdisp_clmr_sbl_set_luxmode(unsigned char luxmode);
static int shdisp_clmr_sbl_on_setting(unsigned char mode, unsigned short ap_type);
static int shdisp_clmr_sbl_on(void);
static int shdisp_clmr_sbl_off(void);
static int shdisp_clmr_svct_on(unsigned short mode, unsigned short ap_type);
static int shdisp_clmr_svct_off(void);
static int shdisp_clmr_hsv_param_set(unsigned short mode, unsigned short ap_type);
static int shdisp_clmr_hsv_on(unsigned short mode, unsigned short ap_type);
static int shdisp_clmr_hsv_off(void);
static int shdisp_clmr_pca_config(unsigned short mode, unsigned short ap_type);
static int shdisp_clmr_pca_on(unsigned short mode, unsigned short ap_type);
static int shdisp_clmr_pca_off(void);
static int shdisp_clmr_cpf_lut_write(unsigned short mode, unsigned short ap_type);
static int shdisp_clmr_cpf_param_set(unsigned short mode, unsigned short ap_type);
static int shdisp_clmr_cpf_on(unsigned short mode);
static int shdisp_clmr_cpf_off(void);
int shdisp_clmr_cpf_lut_rewrite(unsigned short mode, unsigned short ap_type);
static int shdisp_clmr_smite_on(unsigned short mode, unsigned short ap_type);
static int shdisp_clmr_smite_mode_chg(unsigned short mode);
static int shdisp_clmr_smite_off(void);
static int shdisp_clmr_smite_lpmc_setting_chg(unsigned short ap_type);
static int shdisp_clmr_ae_param_set(void);
static int shdisp_clmr_ae_time_set(unsigned char mode);
static int shdisp_clmr_ae_on(void);
static int shdisp_clmr_ae_off(unsigned char mode);
static int shdisp_clmr_flicker_trv_on(unsigned char level, unsigned char type);
static int shdisp_clmr_flicker_trv_off(void);
static int shdisp_clmr_flicker_trv_vsp_on_off(int on);
static int shdisp_clmr_flicker_trv_custom_set(void);
#ifdef KERNEL_CALL_PIC_ADJ_MDP
static int shdisp_clmr_set_pic_adj_data(int mode, unsigned short ap_type);
static void shdisp_mdp_set_hist_lut_data(int mode, struct msmfb_pipe_hist_lut_data *data, unsigned short type);
static void shdisp_mdp_set_qseed_data(int mode, struct msmfb_pipe_qseed_data *data, unsigned short type);
static void shdisp_mdp_set_csc_data(int mode, struct mdp_csc *data, unsigned short type);
#endif
static int shdisp_clmr_register_driver(void);

static void shdisp_clmr_reg_dump_logset(void);
static void shdisp_clmr_pwroff_fwlog_dump(void);
static int shdisp_clmr_power_on(void);
static void shdisp_clmr_tearint_log(void);

extern int shdisp_api_do_psals_recovery(void);

/*---------------------------------------------------------------------------*/
/*      Register Setting                                                     */
/*---------------------------------------------------------------------------*/
const static shdisp_clmrRegSetting_t clock_setting1[] = {
    {SHDISP_CLMR_REG_SYSCTL,        CALI_RMW,   0x02007004, ~0x33307034, 0},
    {SHDISP_CLMR_REG_OSCCTL2,       CALI_STR,   0x02000026,  0x00000000, 0},
    {SHDISP_CLMR_REG_OSCCTL,        CALI_STR,   0x02000081,  0x00000000, 0},
    {SHDISP_CLMR_REG_CLKSELMASK,    CALI_STR,   0x02000003,  0x00000000, 0},

    {SHDISP_CLMR_REG_XCLKDIV,       CALI_STR,   0x03000200,  0x00000000, 0},
    {SHDISP_CLMR_REG_VRMDIV,        CALI_STR,   0x00000003,  0x00000000, 0},
    {SHDISP_CLMR_REG_REFDIV,        CALI_STR,   0x00000200,  0x00000000, 0},
#if defined(CONFIG_SHDISP_PANEL_ANDY)
    {SHDISP_CLMR_REG_PREDIV,        CALI_STR,   0x00000100,  0x00000000, 0},
    {SHDISP_CLMR_REG_LAYDIV,        CALI_STR,   0x00000002,  0x00000000, 0},
    {SHDISP_CLMR_REG_PTGDIV,        CALI_STR,   0x00000002,  0x00000000, 0},
    {SHDISP_CLMR_REG_TXMPDIV,       CALI_STR,   0x01080100,  0x00000000, 0},
    {SHDISP_CLMR_REG_RXMPDIV,       CALI_STR,   0x01080100,  0x00000000, 0},
#elif defined(CONFIG_SHDISP_PANEL_MARCO) || defined(CONFIG_SHDISP_PANEL_CARIN)
    {SHDISP_CLMR_REG_PREDIV,        CALI_STR,   0x00000100,  0x00000000, 0},
    {SHDISP_CLMR_REG_LAYDIV,        CALI_STR,   0x00000002,  0x00000000, 0},
    {SHDISP_CLMR_REG_PTGDIV,        CALI_STR,   0x00000002,  0x00000000, 0},
    {SHDISP_CLMR_REG_TXMPDIV,       CALI_STR,   0x01080100,  0x00000000, 0},
    {SHDISP_CLMR_REG_RXMPDIV,       CALI_STR,   0x01080100,  0x00000000, 0},
#elif defined(CONFIG_SHDISP_PANEL_GEMINI)
    {SHDISP_CLMR_REG_PREDIV,        CALI_STR,   0x00000100,  0x00000000, 0},
    {SHDISP_CLMR_REG_LAYDIV,        CALI_STR,   0x00000002,  0x00000000, 0},
    {SHDISP_CLMR_REG_PTGDIV,        CALI_STR,   0x01000002,  0x00000000, 0},
    {SHDISP_CLMR_REG_TXMPDIV,       CALI_STR,   0x01090100,  0x00000000, 0},
    {SHDISP_CLMR_REG_RXMPDIV,       CALI_STR,   0x01090100,  0x00000000, 0},
#else /* elif defined(CONFIG_SHDISP_PANEL_RYOMA) */
    {SHDISP_CLMR_REG_PREDIV,        CALI_STR,   0x00000004,  0x00000000, 0},
    {SHDISP_CLMR_REG_LAYDIV,        CALI_STR,   0x00000101,  0x00000000, 0},
    {SHDISP_CLMR_REG_PTGDIV,        CALI_STR,   0x00000101,  0x00000000, 0},
    {SHDISP_CLMR_REG_TXMPDIV,       CALI_STR,   0x01080200,  0x00000000, 0},
    {SHDISP_CLMR_REG_RXMPDIV,       CALI_STR,   0x01080004,  0x00000000, 0},
#endif
    {SHDISP_CLMR_REG_TCONTGDIV,     CALI_STR,   0x00000005,  0x00000000, 0},
    {SHDISP_CLMR_REG_I2CDIV,        CALI_STR,   0x00000200,  0x00000000, 0},
#if defined(CONFIG_SHDISP_PANEL_ANDY)
  #if defined(CONFIG_MACH_LYNX_DL50) || defined(CONFIG_MACH_EBZ)
    {SHDISP_CLMR_REG_PWMDIV,        CALI_STR,   0x00000009,  0x00000000, 0},
  #elif defined(CONFIG_MACH_DECKARD_AS99)
    {SHDISP_CLMR_REG_PWMDIV,        CALI_STR,   0x00000008,  0x00000000, 0},
  #else
    {SHDISP_CLMR_REG_PWMDIV,        CALI_STR,   0x00000008,  0x00000000, 0},
  #endif
    {SHDISP_CLMR_REG_LUXDIV,        CALI_STR,   0x00000002,  0x00000000, 0},
#elif defined(CONFIG_SHDISP_PANEL_MARCO) || defined(CONFIG_SHDISP_PANEL_CARIN)
    {SHDISP_CLMR_REG_PWMDIV,        CALI_STR,   0x00000009,  0x00000000, 0},
    {SHDISP_CLMR_REG_LUXDIV,        CALI_STR,   0x00000002,  0x00000000, 0},
#elif defined(CONFIG_SHDISP_PANEL_GEMINI)
    {SHDISP_CLMR_REG_PWMDIV,        CALI_STR,   0x00000009,  0x00000000, 0},
    {SHDISP_CLMR_REG_LUXDIV,        CALI_STR,   0x00000002,  0x00000000, 0},
#else /* elif defined(CONFIG_SHDISP_PANEL_RYOMA) */
    {SHDISP_CLMR_REG_PWMDIV,        CALI_STR,   0x0000000C,  0x00000000, 0},
    {SHDISP_CLMR_REG_LUXDIV,        CALI_STR,   0x00000101,  0x00000000, 0},
#endif
    {SHDISP_CLMR_REG_SYSCTL,        CALI_OR,    0x00000004,  0x00000000, 0},
    {SHDISP_CLMR_REG_PLL1CTL,       CALI_OR,    0x10000000,  0x00000000, 8},
    {SHDISP_CLMR_REG_PLL1CTL,       CALI_AND,   0x00000000, ~0x10000000, 0},
    {SHDISP_CLMR_REG_MCLKDIV,       CALI_AND,   0x00000000, ~0x0300001F, 0},
    {SHDISP_CLMR_REG_PLL1CTL,       CALI_STR,   CALI_PLL1CTL_VAL,  0x00000000, 0},
    {SHDISP_CLMR_REG_PLL1CTL2,      CALI_STR,   CALI_PLL1CTL2_VAL, 0x00000000, 0},
    {SHDISP_CLMR_REG_PLL1CTL3,      CALI_STR,   CALI_PLL1CTL3_VAL, 0x00000000, 0},
    {SHDISP_CLMR_REG_PLL1CTL,       CALI_OR,    0x00000001,  0x00000000, 0},
};

const static shdisp_clmrRegSetting_t clock_setting2[] = {
    {SHDISP_CLMR_REG_SYSCTL,        CALI_RMW,   0x00000020,  0xFFFF8F0F, 0},
    {SHDISP_CLMR_REG_CLKSYS3,       CALI_OR,    0x00000003,  0x00000000, 0},
    {SHDISP_CLMR_REG_REGDIV,        CALI_STR,   0x00000103,  0x00000000, 0},
    {SHDISP_CLMR_REG_CLKSYS3,       CALI_OR,    0x00007000,  0x00000000, 0},
    {SHDISP_CLMR_REG_REFCTL,        CALI_STR,   0x00000001,  0x00000000, 0},
};

const static shdisp_clmrRegSetting_t gpclkInit[] = {
    {SHDISP_CLMR_REG_CLKSYS3,       CALI_AND,   0x00000000, ~0x00000800, 0},
    {SHDISP_CLMR_REG_GIOSEL,        CALI_AND,   0x00000000, ~0x00000004, 0},
    {SHDISP_CLMR_REG_DSCTL,         CALI_OR,    0x00000004,  0x00000000, 0},
};

const static shdisp_clmrRegSetting_t gpclkOn[] = {
    {SHDISP_CLMR_REG_GPDIV,         CALI_STR,   CALI_GPDIV_VAL,  0x00000000, 0},
    {SHDISP_CLMR_REG_CLKSYS3,       CALI_OR,    0x00000800,  0x00000000, 0},
};

const static shdisp_clmrRegSetting_t gpclkOff[] = {
    {SHDISP_CLMR_REG_GPDIV,         CALI_AND,   0x00000000, ~0x00001000, 0},
    {SHDISP_CLMR_REG_CLKSYS3,       CALI_AND,   0x00000000, ~0x00000800, 0},
};

const static shdisp_clmrRegSetting_t arm_init[] = {
#ifdef SHDISP_NOT_SUPPORT_DET
    {SHDISP_CLMR_REG_INTM,          CALI_OR,    0x00010000,  0x00000000, 0},
#else  /* SHDISP_NOT_SUPPORT_DET */
    {SHDISP_CLMR_REG_INTM,          CALI_OR,    0x00030000,  0x00000000, 0},
#endif /* SHDISP_NOT_SUPPORT_DET */
    {SHDISP_CLMR_REG_ARMSRAMCTL,    CALI_STR,   0x00000000,  0x00000000, 0},
    {SHDISP_CLMR_REG_ARMDEBEN,      CALI_STR,   0x00000001,  0x00000000, 0},
    {SHDISP_CLMR_REG_ARMHOSTSYS,    CALI_STR,   0x00000003,  0x00000000, 0},
    {SHDISP_CLMR_REG_ARMCONTCLKEN,  CALI_STR,   0x00000001,  0x00000000, 0},
    {SHDISP_CLMR_REG_ARMSLPCLKEN,   CALI_STR,   0x00000001,  0x00000000, 0},
};

static shdisp_clmrRegSetting_t fw_download[] = {
    {SHDISP_CLMR_REG_HOSTSYS,       CALI_STR,   0x00000012,  0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTCTL,       CALI_STR,   0x00000000,  0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTBASE,      CALI_STR,   CALI_HOSTBASE_VAL,  0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTHW,        CALI_STR,   0x00000001,  0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTASX,       CALI_STR,   0x00000000,  0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTASY,       CALI_STR,   0x00000000,  0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTAEX,       CALI_STR,   0x00000000,  0x00000000, 0},
};

static shdisp_clmrRegSetting_t arm_sram2fw[] = {
    {SHDISP_CLMR_REG_ARMDMA,        CALI_STR,   0x00000010,  0x00000000, 0},
    {SHDISP_CLMR_REG_ARMDMAHW,      CALI_STR,   0x00000001,  0x00000000, 0},
    {SHDISP_CLMR_REG_ARMSRAMBASE,   CALI_STR,   0x00000000,  0x00000000, 0},
    {SHDISP_CLMR_REG_ARMEDRAMBASE,  CALI_STR,   CALI_HOSTBASE_VAL,  0x00000000, 0},
    {SHDISP_CLMR_REG_ARMEDRAMSTART, CALI_STR,   0x00000000,  0x00000000, 0},
    {SHDISP_CLMR_REG_ARMDMASIZE,    CALI_STR,   0x00000080,  0x00000000, 0},
    {SHDISP_CLMR_REG_ARMDMA,        CALI_STR,   0x00000011,  0x00000000, 0},
};

const static shdisp_clmrRegSetting_t arm_boot[] = {
    {SHDISP_CLMR_REG_ARMHOSTSYS,    CALI_STR,   0x00000013,  0x00000000, 0},
};


const static shdisp_clmrRegSetting_t timing_setting[] = {
    {SHDISP_CLMR_REG_PTGHP,         CALI_STRM,  CALI_PTGHP_VAL,         0x00000000, 0},
    {SHDISP_CLMR_REG_PTGHB,         CALI_STRM,  CALI_PTGHB_VAL,         0x00000000, 0},
    {SHDISP_CLMR_REG_PTGHF,         CALI_STRM,  CALI_PTGHF_VAL,         0x00000000, 0},
    {SHDISP_CLMR_REG_PTGVP,         CALI_STRM,  CALI_PTGVP_VAL,         0x00000000, 0},
    {SHDISP_CLMR_REG_PTGVB,         CALI_STRM,  CALI_PTGVB_VAL,         0x00000000, 0},
    {SHDISP_CLMR_REG_PTGVF,         CALI_STRM,  CALI_PTGVF_VAL,         0x00000000, 0},
    {SHDISP_CLMR_REG_PTGHW,         CALI_STRM,  CALI_PTGHW_VAL,         0x00000000, 0},
    {SHDISP_CLMR_REG_PTGVW,         CALI_STRM,  CALI_PTGVW_VAL,         0x00000000, 0},
    {SHDISP_CLMR_REG_PTGSRCXSIZE,   CALI_STRM,  CALI_PTGSRCXSIZE_VAL,   0x00000000, 0},
    {SHDISP_CLMR_REG_PTGSRCYSIZE,   CALI_STRM,  CALI_PTGSRCYSIZE_VAL,   0x00000000, 0},
    {SHDISP_CLMR_REG_PTGDESXSIZE,   CALI_STRM,  CALI_PTGDESXSIZE_VAL,   0x00000000, 0},
    {SHDISP_CLMR_REG_PTGDESYSIZE,   CALI_STRM,  CALI_PTGDESYSIZE_VAL,   0x00000000, 0},
    {SHDISP_CLMR_REG_PTGCTL,        CALI_STRM,  0x00000000,  0x00000000, 0},
    {SHDISP_CLMR_REG_PTGMIFCTL,     CALI_STRM,  0x00000002,  0x00000000, 0},
    {SHDISP_CLMR_REG_PTGVRAMHW,     CALI_STRM,  CALI_PTGVRAMHW_VAL,     0x00000000, 0},
    {SHDISP_CLMR_REG_PTGVRAMBASE,   CALI_STRM,  0x00000000,  0x00000000, 0},
    {SHDISP_CLMR_REG_CLKSYS,        CALI_OR,    0x00000030,  0x00000000, 0},
    {SHDISP_CLMR_REG_CLKSYS3,       CALI_OR,    0x00050000,  0x00000000, 0},
    {SHDISP_CLMR_REG_CSTMSYS,       CALI_STRM,  0x00000017,  0x00000000, 0},
    {SHDISP_CLMR_REG_SYSCTL,        CALI_OR,    0x00000001,  0x00000000, 0},
    {SHDISP_CLMR_REG_CLKSYS3,       CALI_RMW,   0x00000010, ~0x00000030, 0},
    {SHDISP_CLMR_REG_GIOSEL,        CALI_AND,   0x00000000, ~0x00000008, 0},
    {SHDISP_CLMR_REG_PTGVBLKCTL,    CALI_STRM,  0x00000000,  0x00000000, 0},
    {SHDISP_CLMR_REG_PTGVBLKS,      CALI_STRM,  0x00300001,  0x00000000, 0},
    {SHDISP_CLMR_REG_PTGVBLKE,      CALI_STRM,  0x00200001,  0x00000000, 0},
#if defined(CONFIG_SHDISP_PANEL_GEMINI)
    {SHDISP_CLMR_REG_GIOSEL,        CALI_RMW,   0x00001002, ~0x00001003, 0},
    {SHDISP_CLMR_REG_GIO01,         CALI_STR,   0x00000000,  0x00000000, 0},
    {SHDISP_CLMR_REG_GIO12,         CALI_STR,   0x00000000,  0x00000000, 0},
#else
    {SHDISP_CLMR_REG_GIOSEL,        CALI_OR,    0x00000001,  0x00000000, 0},
#endif
    {SHDISP_CLMR_REG_PTGTPSYNCS,    CALI_STRM,  0x00000001,  0x00000000, 0},
    {SHDISP_CLMR_REG_PTGTPSYNCE,    CALI_STRM,  0x00200001,  0x00000000, 0},
    {SHDISP_CLMR_REG_PTGTPSYNCCTL,  CALI_STRM,  0x00000013,  0x00000000, 0},
};

const static shdisp_clmrRegSetting_t auto_pat_on[] = {
    {SHDISP_CLMR_REG_PTGMIFCTL,     CALI_RMW,   0x00001000,  ~0x00007000, 0},
};
const static shdisp_clmrRegSetting_t auto_pat_off[] = {
    {SHDISP_CLMR_REG_PTGMIFCTL,     CALI_AND,   0x00000000,  ~0x00007000, 0},
};

const static shdisp_clmrRegSetting_t arm_reset[] = {
    {SHDISP_CLMR_REG_ARMHOSTSYS,    CALI_AND,   0x00000000,  ~0x00000010, 0}
};

#if defined(CONFIG_SHDISP_PANEL_ANDY)
#define CALI_PREVRAMHW_VAL       101
#define CALI_PRECEX_VAL          1079
#define CALI_PRECEY_VAL          1919
#define CALI_PREXSIZE_VAL        0x00650438
#define CALI_PREYSIZE_VAL        0x07850780
#define CALI_NACOCTL_VAL         0x07800438
#elif defined(CONFIG_SHDISP_PANEL_MARCO) || defined(CONFIG_SHDISP_PANEL_CARIN)
#define CALI_PREVRAMHW_VAL       101
#define CALI_PRECEX_VAL          1079
#define CALI_PRECEY_VAL          1919
#define CALI_PREXSIZE_VAL        0x00650438
#define CALI_PREYSIZE_VAL        0x07850780
#define CALI_NACOCTL_VAL         0x07800438
#elif defined(CONFIG_SHDISP_PANEL_GEMINI)
#define CALI_PREVRAMHW_VAL       (139)
#define CALI_PRECEX_VAL          (1199)
#define CALI_PRECEY_VAL          (1919)
#define CALI_PREXSIZE_VAL        (0x008B04B0)
#define CALI_PREYSIZE_VAL        (0x06120780)
#define CALI_NACOCTL_VAL         (0x078004B0)
#else /* #elif defined(CONFIG_SHDISP_PANEL_RYOMA) */
#define CALI_PREVRAMHW_VAL       135
#define CALI_PRECEX_VAL          719
#define CALI_PRECEY_VAL          1279
#define CALI_PREXSIZE_VAL        0x008702D0
#define CALI_PREYSIZE_VAL        0x05000500
#define CALI_NACOCTL_VAL         0x050002D0
#endif

const static shdisp_clmrRegSetting_t prepro_setting[] = {
    {SHDISP_CLMR_REG_CLKSYS,        CALI_OR,    0x00000001,  0x00000000, 0},
    {SHDISP_CLMR_REG_CLKSYS3,       CALI_OR,    0x00003200,  0x00000000, 0},
    {SHDISP_CLMR_REG_PRESYS,        CALI_STRM,  0x00000001,  0x00000000, 0},
    {SHDISP_CLMR_REG_PRESYS,        CALI_STRM,  0x00000000,  0x00000000, 0},
    {SHDISP_CLMR_REG_PRECSX,        CALI_STRM,  0x00000000,  0x00000000, 0},
    {SHDISP_CLMR_REG_PRECSY,        CALI_STRM,  0x00000000,  0x00000000, 0},
    {SHDISP_CLMR_REG_PRECEX,        CALI_STRM,  CALI_PRECEX_VAL,    0x00000000, 0},
    {SHDISP_CLMR_REG_PRECEY,        CALI_STRM,  CALI_PRECEY_VAL,    0x00000000, 0},
    {SHDISP_CLMR_REG_PREXSIZE,      CALI_STRM,  CALI_PREXSIZE_VAL,  0x00000000, 0},
    {SHDISP_CLMR_REG_PREYSIZE,      CALI_STRM,  CALI_PREYSIZE_VAL,  0x00000000, 0},
    {SHDISP_CLMR_REG_PREVRAMHW,     CALI_STRM,  CALI_PREVRAMHW_VAL, 0x00000000, 0},
    {SHDISP_CLMR_REG_PTGVRAMBASE,   CALI_STRM,  0x00000000,  0x00000000, 0},
    {SHDISP_CLMR_REG_PREMIFCTL,     CALI_STRM,  0x00000002,  0x00000000, 0},
    {SHDISP_CLMR_REG_NACOCTL,       CALI_STRM,  CALI_NACOCTL_VAL,   0x00000000, 0},
};

const static shdisp_clmrRegSetting_t mipi_dsi_rx_setting[] = {
    {SHDISP_CLMR_REG_CLKSYS3,       CALI_OR,    0x30000000,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDRMSYS,       CALI_OR,    0x00000010,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDRMSYS,       CALI_RMW,   0x00000003, ~0x00000010, 0},
    {SHDISP_CLMR_REG_MDRMCTL1,      CALI_RMW,   0x00000E1C, ~0x00000E1C, 0},
    {SHDISP_CLMR_REG_MDRMCTL2,      CALI_STRM,  0x00000007,  0x00000000, 0},
#if defined(CONFIG_SHDISP_PANEL_ANDY) || defined(CONFIG_SHDISP_PANEL_GEMINI)
    {SHDISP_CLMR_REG_MDRMDPHYCL,    CALI_STRM,  0x00000038,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDRMDPHYDL,    CALI_STRM,  0x00000008,  0x00000000, 0},
#elif defined(CONFIG_SHDISP_PANEL_MARCO) || defined(CONFIG_SHDISP_PANEL_CARIN)
    {SHDISP_CLMR_REG_MDRMDPHYCL,    CALI_STRM,  0x00000038,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDRMDPHYDL,    CALI_STRM,  0x00000008,  0x00000000, 0},
    {SHDISP_CLMR_REG_RXBUFCTL2,     CALI_RMW,   0x00000000, ~0x00000C00, 0},
#else /* elif defined(CONFIG_SHDISP_PANEL_RYOMA) */
    {SHDISP_CLMR_REG_MDRMDPHYCL,    CALI_STRM,  0x00000036,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDRMDPHYDL,    CALI_STRM,  0x00000006,  0x00000000, 0},
#endif
    {SHDISP_CLMR_REG_MDRMCTL1,      CALI_OR,    0x00000001,  0x00000000, 0},
};

#if defined(CONFIG_SHDISP_PANEL_GEMINI)
const static shdisp_clmrRegSetting_t tcon_setting[] = {
    {SHDISP_CLMR_REG_CLKSYS3,       CALI_OR,    0x00020000,  0x00000000, 0},
    {SHDISP_CLMR_REG_CLKSYS,        CALI_OR,    0x00000040,  0x00000000, 0},
    {SHDISP_CLMR_REG_TCSYS,         CALI_OR,    0x00000010,  0x00000000, 100},
    {SHDISP_CLMR_REG_TCSYS,         CALI_AND,   0x00000000, ~0x00000010, 0},
    {SHDISP_CLMR_REG_TCCTL,         CALI_OR,    0x00000081,  0x00000000, 0},
    {SHDISP_CLMR_REG_TCTHP,         CALI_STRM,  0x00000002,  0x00000000, 0},
    {SHDISP_CLMR_REG_TCTHB,         CALI_STRM,  0x00000008,  0x00000000, 0},
    {SHDISP_CLMR_REG_TCTHW0,        CALI_STRM,  0x0000025B,  0x00000000, 0},
    {SHDISP_CLMR_REG_TCTHW1,        CALI_STRM,  0x0000025B,  0x00000000, 0},
    {SHDISP_CLMR_REG_TCDELAY,       CALI_STRM,  0x0000025B,  0x00000000, 0},
    {SHDISP_CLMR_REG_TCTHFMIN,      CALI_STRM,  0x00000059,  0x00000000, 0},
    {SHDISP_CLMR_REG_TCHW0OFS,      CALI_STRM,  0x00000000,  0x00000000, 0},
    {SHDISP_CLMR_REG_TCHW1OFS,      CALI_STRM,  0x00000255,  0x00000000, 0},
    {SHDISP_CLMR_REG_TCTVP,         CALI_STRM,  0x00000000,  0x00000000, 0},
};
#endif

#if defined(CONFIG_SHDISP_PANEL_ANDY)
    #define CALI_MDTMHSABYTE_VAL        14
    #define CALI_MDTMHBPBYTE_VAL        62
  #if defined(CONFIG_MACH_LYNX_DL50)
    #define CALI_MDTMHFPBYTE_VAL        204
    #define CALI_MDTMHBLBYTE_VAL        3518
  #elif defined(CONFIG_MACH_DECKARD_AS99)
    #define CALI_MDTMHFPBYTE_VAL        162
    #define CALI_MDTMHBLBYTE_VAL        3476
  #elif defined(CONFIG_MACH_EBZ)
    #define CALI_MDTMHFPBYTE_VAL        204
    #define CALI_MDTMHBLBYTE_VAL        3518
  #else
    #define CALI_MDTMHFPBYTE_VAL        372
    #define CALI_MDTMHBLBYTE_VAL        3686
  #endif
    #define CALI_MDTMHWBYTE_VAL         3240
    #define CALI_MDTMTHB_VAL            16
    #define CALI_MDTMTHW_VAL            1080
    #define CALI_MDTMTVB_VAL            6
    #define CALI_MDTMTVW_VAL            1920
#elif defined(CONFIG_SHDISP_PANEL_MARCO) || defined(CONFIG_SHDISP_PANEL_CARIN)
    #define CALI_MDTMHSABYTE_VAL        38
    #define CALI_MDTMHBPBYTE_VAL        170
    #define CALI_MDTMHFPBYTE_VAL        348
    #define CALI_MDTMHBLBYTE_VAL        3770
    #define CALI_MDTMHWBYTE_VAL         3240
    #define CALI_MDTMTHB_VAL            44
    #define CALI_MDTMTHW_VAL            1080
    #define CALI_MDTMTVB_VAL            10
    #define CALI_MDTMTVW_VAL            1920
#elif defined(CONFIG_SHDISP_PANEL_GEMINI)
    #define CALI_MDTMHSABYTE_VAL        (0)
    #define CALI_MDTMHBPBYTE_VAL        (20)
    #define CALI_MDTMHFPBYTE_VAL        (255)
    #define CALI_MDTMHBLBYTE_VAL        (2096)
    #define CALI_MDTMHWBYTE_VAL         (1809)
    #define CALI_MDTMTHB_VAL            (16)
    #define CALI_MDTMTHW_VAL            (1200)
    #define CALI_MDTMTVB_VAL            (7)
    #define CALI_MDTMTVW_VAL            (1920)
#else /* #elif defined(CONFIG_SHDISP_PANEL_RYOMA) */
    #define CALI_MDTMHSABYTE_VAL        38
    #define CALI_MDTMHBPBYTE_VAL        170
    #define CALI_MDTMHFPBYTE_VAL        375
    #define CALI_MDTMHBLBYTE_VAL        2717
    #define CALI_MDTMHWBYTE_VAL         2160
    #define CALI_MDTMTHB_VAL            44
    #define CALI_MDTMTHW_VAL            720
    #define CALI_MDTMTVB_VAL            6
    #define CALI_MDTMTVW_VAL            1280
#endif /* defined(CONFIG_SHDISP_PANEL_ANDY) */

const static shdisp_clmrRegSetting_t mipi_dsi_tx_setting[] = {
    {SHDISP_CLMR_REG_CLKSYS3,       CALI_OR,    0xC0000000,  0x00000000, 0},
    {SHDISP_CLMR_REG_TXSYS,         CALI_OR,    0x00000200,  0x00000000, 0},
    {SHDISP_CLMR_REG_TXSYS,         CALI_RMW,   0x00000000, ~0x00000200, 0},
    {SHDISP_CLMR_REG_TXBUFCTL,      CALI_STRM,  0x00000000,  0x00000000, 0},
#if defined(CONFIG_SHDISP_PANEL_GEMINI)
    {SHDISP_CLMR_REG_TXBUFCTL2,     CALI_STRM,  0x00002702,  0x00000000, 0},
#else
    {SHDISP_CLMR_REG_TXBUFCTL2,     CALI_STRM,  0x00002500,  0x00000000, 0},
#endif
    {SHDISP_CLMR_REG_TXSYS,         CALI_OR,    0x00000030,  0x00000000, 0},
#if defined(CONFIG_SHDISP_PANEL_GEMINI)
    {SHDISP_CLMR_REG_MDTMCTL,       CALI_RMW,   0x00000004, ~0x000001BC, 0},
    {SHDISP_CLMR_REG_MDTMCTL2,      CALI_RMW,   0x000025C2, ~0x0000FFFE, 0},
#else
    {SHDISP_CLMR_REG_MDTMCTL,       CALI_RMW,   0x0000000C, ~0x000001BC, 0},
    {SHDISP_CLMR_REG_MDTMCTL2,      CALI_RMW,   0x00002DC2, ~0x0000FFFE, 0},
#endif
    {SHDISP_CLMR_REG_MDTMHSABYTE,   CALI_STRM,  CALI_MDTMHSABYTE_VAL,   0x00000000, 0},
    {SHDISP_CLMR_REG_MDTMHBPBYTE,   CALI_STRM,  CALI_MDTMHBPBYTE_VAL,   0x00000000, 0},
    {SHDISP_CLMR_REG_MDTMHFPBYTE,   CALI_STRM,  CALI_MDTMHFPBYTE_VAL,   0x00000000, 0},
    {SHDISP_CLMR_REG_MDTMHBLBYTE,   CALI_STRM,  CALI_MDTMHBLBYTE_VAL,   0x00000000, 0},
    {SHDISP_CLMR_REG_MDTMHWBYTE,    CALI_STRM,  CALI_MDTMHWBYTE_VAL,    0x00000000, 0},
    {SHDISP_CLMR_REG_MDTMTHB,       CALI_STRM,  CALI_MDTMTHB_VAL,       0x00000000, 0},
    {SHDISP_CLMR_REG_MDTMTHW,       CALI_STRM,  CALI_MDTMTHW_VAL,       0x00000000, 0},
    {SHDISP_CLMR_REG_MDTMTVB,       CALI_STRM,  CALI_MDTMTVB_VAL,       0x00000000, 0},
    {SHDISP_CLMR_REG_MDTMTVW,       CALI_STRM,  CALI_MDTMTVW_VAL,       0x00000000, 0},
#if defined(CONFIG_SHDISP_PANEL_ANDY)
  #if defined(CONFIG_MACH_EBZ)
    {SHDISP_CLMR_REG_MDTMDPHYCL1,   CALI_STRM,  0x0C1C0604,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDTMDPHYCL2,   CALI_STRM,  0x00000905,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDTMDPHYDL1,   CALI_STRM,  0x000C0604,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDTMDPHYDL2,   CALI_STRM,  0x00000906,  0x00000000, 0},
  #else
    {SHDISP_CLMR_REG_MDTMDPHYCL1,   CALI_STRM,  0x161C0606,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDTMDPHYCL2,   CALI_STRM,  0x00000A06,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDTMDPHYDL1,   CALI_STRM,  0x000C0606,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDTMDPHYDL2,   CALI_STRM,  0x00000A06,  0x00000000, 0},
  #endif
#elif defined(CONFIG_SHDISP_PANEL_MARCO) || defined(CONFIG_SHDISP_PANEL_CARIN)
    {SHDISP_CLMR_REG_MDTMDPHYCL1,   CALI_OR,    0x0C1C0604,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDTMDPHYCL2,   CALI_OR,    0x00000905,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDTMDPHYDL1,   CALI_OR,    0x000C0604,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDTMDPHYDL2,   CALI_OR,    0x00000906,  0x00000000, 0},
#elif defined(CONFIG_SHDISP_PANEL_GEMINI)
    {SHDISP_CLMR_REG_MDTMDPHYCL1,   CALI_STRM,  0x0E200506,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDTMDPHYCL2,   CALI_STRM,  0x00000D08,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDTMDPHYDL1,   CALI_STRM,  0x000E0606,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDTMDPHYDL2,   CALI_STRM,  0x00000D08,  0x00000000, 0},
#else /* elif defined(CONFIG_SHDISP_PANEL_RYOMA) */
    {SHDISP_CLMR_REG_MDTMDPHYCL1,   CALI_STRM,  0x090D0202,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDTMDPHYCL2,   CALI_STRM,  0x00000503,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDTMDPHYDL1,   CALI_STRM,  0x00060202,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDTMDPHYDL2,   CALI_STRM,  0x00000503,  0x00000000, 0},
#endif
#if defined(CONFIG_SHDISP_PANEL_GEMINI)
    {SHDISP_CLMR_REG_MDTM2CTL,      CALI_RMW,   0x00000004, ~0x000001BC, 0},
    {SHDISP_CLMR_REG_MDTM2CTL2,     CALI_RMW,   0x000025C2, ~0x0000FFFE, 0},

    {SHDISP_CLMR_REG_MDTM2HSABYTE,  CALI_STRM,  CALI_MDTMHSABYTE_VAL,   0x00000000, 0},
    {SHDISP_CLMR_REG_MDTM2HBPBYTE,  CALI_STRM,  CALI_MDTMHBPBYTE_VAL,   0x00000000, 0},
    {SHDISP_CLMR_REG_MDTM2HFPBYTE,  CALI_STRM,  CALI_MDTMHFPBYTE_VAL,   0x00000000, 0},
    {SHDISP_CLMR_REG_MDTM2HBLBYTE,  CALI_STRM,  CALI_MDTMHBLBYTE_VAL,   0x00000000, 0},
    {SHDISP_CLMR_REG_MDTM2HWBYTE,   CALI_STRM,  CALI_MDTMHWBYTE_VAL,    0x00000000, 0},
    {SHDISP_CLMR_REG_MDTM2THB,      CALI_STRM,  CALI_MDTMTHB_VAL,       0x00000000, 0},
    {SHDISP_CLMR_REG_MDTM2THW,      CALI_STRM,  CALI_MDTMTHW_VAL,       0x00000000, 0},
    {SHDISP_CLMR_REG_MDTM2TVB,      CALI_STRM,  CALI_MDTMTVB_VAL,       0x00000000, 0},
    {SHDISP_CLMR_REG_MDTM2TVW,      CALI_STRM,  CALI_MDTMTVW_VAL,       0x00000000, 0},

    {SHDISP_CLMR_REG_MDTM2DPHYCL1,  CALI_STRM,  0x0E200506,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDTM2DPHYCL2,  CALI_STRM,  0x00000D08,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDTM2DPHYDL1,  CALI_STRM,  0x000E0606,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDTM2DPHYDL2,  CALI_STRM,  0x00000D08,  0x00000000, 0},
#endif
};

const static shdisp_clmrRegSetting_t mipi_dsi_tx_circuit_on[] = {
    {SHDISP_CLMR_REG_MDTMCTL,       CALI_OR,    0x00000001,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDTMCTL2,      CALI_OR,    0x00000001,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDTMCLKCTL,    CALI_STRM,  0x00000011,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDTMCLKCTL,    CALI_STRM,  0x00000019,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDTMCLKCTL,    CALI_STRM,  0x0000001B,  0x00000000, 0},
#if defined(CONFIG_SHDISP_PANEL_GEMINI)
    {SHDISP_CLMR_REG_MDTM2CTL,      CALI_OR,    0x00000001,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDTM2CTL2,     CALI_OR,    0x00000001,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDTM2CLKCTL,   CALI_STRM,  0x00000011,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDTM2CLKCTL,   CALI_STRM,  0x00000019,  0x00000000, 0},
    {SHDISP_CLMR_REG_MDTM2CLKCTL,   CALI_STRM,  0x0000001B,  0x00000000, 0},
#endif
};

const static shdisp_clmrRegSetting_t data_transfer_starts[] = {
    {SHDISP_CLMR_REG_MDTMCTL,       CALI_OR,    0x00000080,  0x00000000, 0},
#if defined(CONFIG_SHDISP_PANEL_GEMINI)
    {SHDISP_CLMR_REG_MDTM2CTL,      CALI_OR,    0x00000080,  0x00000000, 0},
#endif
    {SHDISP_CLMR_REG_PSTCTL,        CALI_OR,    0x00000003,  0x00000000, 0},
};

const static shdisp_clmrRegSetting_t data_transfer_stops[] = {
    {SHDISP_CLMR_REG_PSTCTL,        CALI_AND,   0x00000000, ~0x00000003, (WAIT_1FRAME_US * 1)},
#if defined(CONFIG_SHDISP_PANEL_GEMINI)
    {SHDISP_CLMR_REG_MDTM2CTL,      CALI_AND,   0x00000000, ~0x00000080, 0},
#endif
    {SHDISP_CLMR_REG_MDTMCTL,       CALI_AND,   0x00000000, ~0x00000080, 0},
};

const static shdisp_clmrRegSetting_t mipi_dsi_tx_circuit_off[] = {
    {SHDISP_CLMR_REG_MDTMCTL2,      CALI_AND,   0x00000000, ~0x00000001, 0},
    {SHDISP_CLMR_REG_MDTMCTL,       CALI_AND,   0x00000000, ~0x00000001, 0},
#if defined(CONFIG_SHDISP_PANEL_GEMINI)
    {SHDISP_CLMR_REG_MDTM2CTL2,     CALI_AND,   0x00000000, ~0x00000001, 0},
    {SHDISP_CLMR_REG_MDTM2CTL,      CALI_AND,   0x00000000, ~0x00000001, 0},
#endif
    {SHDISP_CLMR_REG_TXSYS,         CALI_OR,    0x00000200,  0x00000000, 0},
};

const static shdisp_clmrRegSetting_t mipi_dsi_rx_circuit_off[] = {
    {SHDISP_CLMR_REG_MDRMCTL1,      CALI_AND,   0x00000000, ~0x00000001, 0},
    {SHDISP_CLMR_REG_MDRMSYS,       CALI_RMW,   0x00000010, ~0x00000003, 0},
    {SHDISP_CLMR_REG_CLKSYS,        CALI_AND,   0x00000000, ~0x00000031, 0},
    {SHDISP_CLMR_REG_CLKSYS3,       CALI_AND,   0x00000000, ~0xF0050230, 0},
};

const static shdisp_clmrRegSetting_t clock_stop[] = {
    {SHDISP_CLMR_REG_REFCTL,        CALI_STR,   0x00000000,  0x00000000, 0},
    {SHDISP_CLMR_REG_CLKSYS3,       CALI_AND,   0x00000000, ~0x00007000, 0},
    {SHDISP_CLMR_REG_REGDIV,        CALI_STR,   0x00000000,  0x00000000, 0},
    {SHDISP_CLMR_REG_SYSCTL,        CALI_AND,   0x00000000, ~0x00000020, 0},
    {SHDISP_CLMR_REG_CLKSYS3,       CALI_RMW,   0x00000001, ~0x00000002, 0},
    {SHDISP_CLMR_REG_PLL1CTL,       CALI_AND,   0x00000000, ~0x00000001, 0},
    {SHDISP_CLMR_REG_SYSCTL,        CALI_RMW,   0x00100000, ~0x00300004, 0},
    {SHDISP_CLMR_REG_CLKSELMASK,    CALI_OR,    0x00000001,  0x00000000, 0},
};

#if !defined(CONFIG_SHDISP_PANEL_GEMINI)
const static unsigned char rate_check_mode_on[2]  = { 0x01, 0x00 };
const static unsigned char rate_check_mode_off[2] = { 0x00, 0x00 };

const static unsigned short vcom_tracking[9] = {
    0x0000,
    0x0E10,
    0x1C20,
    0x5460,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
};

#endif  /* CONFIG_SHDISP_PANEL_GEMINI */

#define CALI_AWHS_VAL       ((CALI_PTGVW_VAL << 16) | CALI_PTGHW_VAL)
#define CALI_VSPCTRL9_VAL   ((CALI_PTGVP_VAL + CALI_PTGVB_VAL + \
                                CALI_PTGVF_VAL + CALI_PTGVW_VAL - 1) << 16)

const static shdisp_clmrRegSetting_t custom_vsp_init[] = {
    {SHDISP_CLMR_CUST_AWHS,         CALI_STRM,  CALI_AWHS_VAL,      0x00000000, 0},
    {SHDISP_CLMR_CUST_SVCTASAXU,    CALI_STRM,  0x00000000,         0x00000000, 0},
    {SHDISP_CLMR_CUST_SVCTASAYU,    CALI_STRM,  0x00000000,         0x00000000, 0},
    {SHDISP_CLMR_CUST_SVCTAWSU,     CALI_STRM,  CALI_PTGHW_VAL,     0x00000000, 0},
    {SHDISP_CLMR_CUST_SVCTAHSU,     CALI_STRM,  CALI_PTGVW_VAL,     0x00000000, 0},
    {SHDISP_CLMR_CUST_SVCTASAXL,    CALI_STRM,  0x00000000,         0x00000000, 0},
    {SHDISP_CLMR_CUST_SVCTASAYL,    CALI_STRM,  0x00000000,         0x00000000, 0},
    {SHDISP_CLMR_CUST_SVCTAWSL,     CALI_STRM,  CALI_PTGHW_VAL,     0x00000000, 0},
    {SHDISP_CLMR_CUST_SVCTAHSL,     CALI_STRM,  CALI_PTGVW_VAL,     0x00000000, 0},
    {SHDISP_CLMR_CUST_STINT,        CALI_OR,    0x00000200,         0x00000000, 0},
    {SHDISP_CLMR_CUST_OUTPUTSEL,    CALI_OR,    0x00010000,         0x00000000, 0},
    {SHDISP_CLMR_CUST_VSPCTRL9,     CALI_STR,   CALI_VSPCTRL9_VAL,  0x00000000, 0},
    {SHDISP_CLMR_CUST_VSPCTRL8,     CALI_RMW,   0x00000004,        ~0x0000000C, 0},
};

const static unsigned char shdisp_fwcmd_clmrVSP_on[10] = { 0x38, 0x80,
                                                               0x00, 0x00, 0x80, 0x00,
                                                               0x00, 0x00, 0x80, 0x00
                                                             };
const static unsigned char shdisp_fwcmd_clrmVSP_funcon[2] = { 0x02, 0x00 };

const static shdisp_clmrRegSetting_t hsclk_on[] = {
    {SHDISP_CLMR_REG_MDTMCTL,       CALI_OR,    0x00000080,  0x00000000, 0},
#if defined(CONFIG_SHDISP_PANEL_GEMINI)
    {SHDISP_CLMR_REG_MDTM2CTL,      CALI_OR,    0x00000080,  0x00000000, 0},
#endif
};

const static shdisp_clmrRegSetting_t hsclk_off[] = {
#if defined(CONFIG_SHDISP_PANEL_GEMINI)
    {SHDISP_CLMR_REG_MDTM2CTL,      CALI_AND,   0x00000000, ~0x00000080, 0},
#endif
    {SHDISP_CLMR_REG_MDTMCTL,       CALI_AND,   0x00000000, ~0x00000080, 0},
};

#define CALI_SBLSIZE0_VAL       ((CALI_PTGVW_VAL << 16) | CALI_PTGHW_VAL)
const static shdisp_clmrRegSetting_t custom_sbl_on_setting_common[] = {
    {SHDISP_CLMR_CUST_SBLFMT,       CALI_STR,    0x0000220b,        0x00000000, 0},
    {SHDISP_CLMR_CUST_SBLSIZE0,     CALI_STR,    CALI_SBLSIZE0_VAL, 0x00000000, 0},
    {SHDISP_CLMR_CUST_SBLSIZE1,     CALI_STR,    0x00000000,        0x00000000, 0},
};
const static shdisp_clmrRegSetting_t custom_ewb_start[] = {
    {SHDISP_CLMR_CUST_EWBPAT0,      CALI_STR,   0x00000000,  0x00000000, 0},
    {SHDISP_CLMR_CUST_EWBPAT1,      CALI_STR,   0x84214812,  0x00000000, 0},
    {SHDISP_CLMR_CUST_EWBPAT2,      CALI_STR,   0xC33C5AA5,  0x00000000, 0},
    {SHDISP_CLMR_CUST_EWBPAT3,      CALI_STR,   0xE7DB7DBE,  0x00000000, 0},
    {SHDISP_CLMR_CUST_EWBEX,        CALI_RMW,   0x00002000, ~0x00007080, 0},
};

static shdisp_clmrRegSetting_t custom_ewb_set[] = {
    {SHDISP_CLMR_CUST_VSPCTRL2,     CALI_RMW,   0x00000008, ~0x00000008, 0},
    {SHDISP_CLMR_CUST_EWBLUTADR,    CALI_STR,   0x00000000,  0x00000000, 0},
    {SHDISP_CLMR_CUST_EWBEX,        CALI_RMW,   0x00000080, ~0x00000080, 0},
    {SHDISP_CLMR_CUST_EWBWRDAT,     CALI_STR,   0x00000000,  0x00000000, 0},
    {SHDISP_CLMR_CUST_EWBEX,        CALI_RMW,   0x00000000, ~0x00000080, 0},
    {SHDISP_CLMR_CUST_VSPCTRL2,     CALI_RMW,   0x00000000, ~0x00000008, 0},
};

const static shdisp_clmrRegSetting_t custom_ewb_on[] = {
    {SHDISP_CLMR_CUST_VSPCTRL1,     CALI_RMW,   0x00080000, ~0x00080000, 0},
};

const static shdisp_clmrRegSetting_t custom_ewb_off[] = {
    {SHDISP_CLMR_CUST_VSPCTRL1,     CALI_RMW,   0x00000000, ~0x00080000, 0},
};

static shdisp_clmrRegSetting_t custom_ewb_control_for_read1[] = {
    {SHDISP_CLMR_CUST_VSPCTRL2,     CALI_RMW,   0x00000008, ~0x00000008, 0},
    {SHDISP_CLMR_REG_HOSTSYS,       CALI_RMW,   0x00000002, ~0x00000002, 0},
    {SHDISP_CLMR_CUST_EWBLUTADR,    CALI_STR,   0x00000000,  0x00000000, 0},
    {SHDISP_CLMR_CUST_EWBEX,        CALI_RMW,   0x00000080, ~0x00000080, 0},
};

const static shdisp_clmrRegSetting_t custom_ewb_control_for_read2[] = {
    {SHDISP_CLMR_CUST_EWBEX,        CALI_RMW,   0x00000000, ~0x00000080, 0},
    {SHDISP_CLMR_REG_HOSTSYS,       CALI_RMW,   0x00000000, ~0x00000002, 0},
    {SHDISP_CLMR_CUST_VSPCTRL2,     CALI_RMW,   0x00000000, ~0x00000008, 0},
};

static shdisp_clmrRegSetting_t custom_trv_initial_setting[] = {
    {SHDISP_CLMR_REG_TRVHW,         CALI_STR,   0x00000000,  0x00000000, 0},
    {SHDISP_CLMR_CUST_TRVRAMHW,     CALI_RMW,   0x00010000, ~0x00010FFF, 0},
};

#if defined(CONFIG_SHDISP_PANEL_GEMINI)
  #define CALI_TRVTX_XSIZE      (300)
  #define CALI_TRVTX_YSIZE      (480)
#elif defined(CONFIG_SHDISP_PANEL_RYOMA)
  #define CALI_TRVTX_XSIZE      (180)
  #define CALI_TRVTX_YSIZE      (320)
#else
  #define CALI_TRVTX_XSIZE      (270)
  #define CALI_TRVTX_YSIZE      (480)
#endif
#define CALI_TRVAWHS0_VAL       (((CALI_PTGVW_VAL   - 1) << 16) | (CALI_PTGHW_VAL   - 1))
#define CALI_TRVCFG_VAL         (((CALI_TRVTX_YSIZE - 1) << 16) | (CALI_TRVTX_XSIZE - 1))
static shdisp_clmrRegSetting_t custom_trv_set_parm[] = {
    {SHDISP_CLMR_CUST_TRVOSTFLT0,   CALI_STR,   0x269436CA,             0x00000000, 0},
    {SHDISP_CLMR_CUST_TRVOSTFLT1,   CALI_STR,   0x13050303,             0x00000000, 0},
    {SHDISP_CLMR_CUST_TRVTEXOFT,    CALI_STR,   0x33000000,             0x00000000, 0},
    {SHDISP_CLMR_CUST_TRVPAT0,      CALI_STR,   0x01C70E38,             0x00000000, 0},
    {SHDISP_CLMR_CUST_TRVPAT1,      CALI_STR,   0x01C70E38,             0x00000000, 0},
    {SHDISP_CLMR_CUST_TRVFPAT0,     CALI_STR,   0x0B5204AD,             0x00000000, 0},
    {SHDISP_CLMR_CUST_TRVFPAT1,     CALI_STR,   0x04AD0B52,             0x00000000, 0},
    {SHDISP_CLMR_CUST_TRVASAXY0,    CALI_STR,   0x00000000,             0x00000000, 0},
    {SHDISP_CLMR_CUST_TRVAWHS0,     CALI_STR,   CALI_TRVAWHS0_VAL,      0x00000000, 0},
    {SHDISP_CLMR_CUST_TRVCTL,       CALI_STR,   0x03110102,             0x00000000, 0},
    {SHDISP_CLMR_CUST_TRVCSCREF,    CALI_RMW,   0x00000000,            ~0x0FFF0000, 0},
    {SHDISP_CLMR_CUST_TRVCFG,       CALI_STR,   CALI_TRVCFG_VAL,        0x00000000, 0},
    {SHDISP_CLMR_CUST_TRVTEXADR0,   CALI_STR,   SHDISP_CLMR_TRV_BASE,   0x00000000, 0},
};

const static shdisp_clmrRegSetting_t custom_trv_filter_off[] = {
    {SHDISP_CLMR_CUST_TRVCTL,       CALI_STR,   0x03000102,  0x00000000, 0},
};

const static shdisp_clmrRegSetting_t custom_trv_filter_on[] = {
    {SHDISP_CLMR_CUST_TRVCTL,       CALI_STR,   0x03110102,  0x00000000, 0},
};

static shdisp_clmrRegSetting_t custom_svct_on[] = {
    {SHDISP_CLMR_CUST_VSPCTRL3,     CALI_STR,    0x00000700, ~0x00000700, 0},
    {SHDISP_CLMR_CUST_VSPCTRL5,     CALI_RMW,    0x00009000, ~0x00009000, 0},
    {SHDISP_CLMR_CUST_VSPCTRL5,     CALI_RMW,    0x00000300, ~0x00000300, 0},
};

const static shdisp_clmrRegSetting_t custom_svct_off[] = {
    {SHDISP_CLMR_CUST_VSPCTRL3,     CALI_RMW,    0x00000000, ~0x00000700, 0},
    {SHDISP_CLMR_CUST_VSPCTRL5,     CALI_RMW,    0x00000000, ~0x00009000, 0},
    {SHDISP_CLMR_CUST_VSPCTRL5,     CALI_RMW,    0x00000000, ~0x00000300, 0},
};

const static shdisp_clmrRegSetting_t custom_flicker_custom_set1[] = {
    {SHDISP_CLMR_REG_CSTMSYS,       CALI_RMW,    0x00000003, ~0x00000003, 0},
};

#define CALI_AWHS_VAL           ((CALI_PTGVW_VAL << 16) | CALI_PTGHW_VAL)
const static shdisp_clmrRegSetting_t custom_flicker_custom_set2[] = {
    {SHDISP_CLMR_REG_SYSCTL,        CALI_RMW,    0x00000001,       ~0x00000001, 0},
    {SHDISP_CLMR_CUST_AWHS,         CALI_STRM,   CALI_AWHS_VAL,     0x00000000, 0},
    {SHDISP_CLMR_CUST_STINT,        CALI_STRM,   0x00000200,        0x00000000, 0},
};

const static shdisp_clmrRegSetting_t custom_flicker_custom_set3[] = {
    {SHDISP_CLMR_CUST_TRVCSCREF,    CALI_RMW,    0x009B0000, ~0x0FFF0000, 0},
    {SHDISP_CLMR_CUST_VSPCTRL1,     CALI_RMW,    0x00800000, ~0x00800000, 0},
};

const static shdisp_clmrRegSetting_t custom_flicker_custom_set4[] = {
    {SHDISP_CLMR_CUST_OUTPUTSEL,    CALI_STR,    0x00010000,  0x00000000, 0},
};

const static shdisp_clmrRegSetting_t custom_flicker_trv_set_lut_adr[] = {
    {SHDISP_CLMR_CUST_TRVLUTADR,    CALI_STR,    0x00000170,  0x00000000, 0},
    {SHDISP_CLMR_CUST_VSPCTRL2,     CALI_RMW,    0x00000002, ~0x00000002, 0},
};

static shdisp_clmrRegSetting_t custom_flicker_trv_set_lut_data[] = {
    {SHDISP_CLMR_CUST_TRVDATA,      CALI_STR,    0x00000000,  0x00000000, 0},
    {SHDISP_CLMR_CUST_TRVDATA,      CALI_STR,    0x00000000,  0x00000000, 0},
};

const static shdisp_clmrRegSetting_t custom_flicker_trv_set_lut3[] = {
    {SHDISP_CLMR_CUST_VSPCTRL2,     CALI_RMW,    0x00000002, ~0x00000002, 0},
    {SHDISP_CLMR_CUST_TRVLUTADR,    CALI_STR,    0x00000000,  0x00000000, 0},
};

static shdisp_clmrRegSetting_t custom_flicker_trv_set_trv[] = {
    {SHDISP_CLMR_CUST_TRVASAXY0,    CALI_STRM,   0x00000000,        0x00000000, 0},
    {SHDISP_CLMR_CUST_TRVAWHS0,     CALI_STRM,   CALI_TRVAWHS0_VAL, 0x00000000, 0},
    {SHDISP_CLMR_CUST_TRVCTL,       CALI_STRM,   0x01000041,        0x00000000, 0},
    {SHDISP_CLMR_CUST_TRVTEXOFT,    CALI_STRM,   0x33000000,        0x00000000, 0},
    {SHDISP_CLMR_CUST_TRVPAT0,      CALI_STRM,   0x00000000,        0x00000000, 0},
    {SHDISP_CLMR_CUST_TRVPAT1,      CALI_STRM,   0x00000000,        0x00000000, 0},
    {SHDISP_CLMR_CUST_TRVEX,        CALI_STRM,   0xFFFF0000,        0x00000000, 0},
};

const static shdisp_clmrRegSetting_t custom_flicker_trv_on[] = {
    {SHDISP_CLMR_CUST_VSPCTRL2,     CALI_RMW,    0x00000080, ~0x00000080, 0},
};

static shdisp_clmrRegSetting_t custom_flicker_trv_vsp_on[] = {
    {SHDISP_CLMR_CUST_VSPCTRL1,     CALI_RMW,    0x00000000, ~0x00180000, 0},
};
#ifdef SHDISP_CLMR_FW_TIMEOUT_DUMP
#define  CALI_HOSTAEY_VAL           ((CALI_END_VAL - CALI_LOGAREA_VAL) * 7 / 2 - 1)
#define  CALI_LOGAREA_VAL_2         ((CALI_END_VAL - CALI_LOGAREA_VAL) / 2 + CALI_LOGAREA_VAL)

const static shdisp_clmrRegSetting_t eDRAM_dump_setting_FW_stop[] = {
    {SHDISP_CLMR_REG_ARMSETINT1,    CALI_STR,   0x00000008,         0x00000000, 0},
    {SHDISP_CLMR_REG_ARMHOSTSYS,    CALI_STR,   0x00000000,         0x00000000, 0},
};

const static shdisp_clmrRegSetting_t eDRAM_dump_setting_1st[] = {
    {SHDISP_CLMR_REG_HOSTSYS,       CALI_STR,   0x00000012,         0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTCTL,       CALI_STR,   0x00000000,         0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTCTL,       CALI_STR,   0x00000001,         0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTCTL,       CALI_STR,   0x00000000,         0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTBASE,      CALI_STR,   CALI_LOGAREA_VAL,   0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTHW,        CALI_STR,   0x00000001,         0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTASX,       CALI_STR,   0x00000000,         0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTASY,       CALI_STR,   0x00000000,         0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTAEX,       CALI_STR,   0x00000000,         0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTAEY,       CALI_STR,   CALI_HOSTAEY_VAL,   0x00000000, 0}
};

const static shdisp_clmrRegSetting_t eDRAM_dump_setting_2nd[] = {
    {SHDISP_CLMR_REG_HOSTCTL,       CALI_STR,   0x00000000,         0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTCTL,       CALI_STR,   0x00000001,         0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTCTL,       CALI_STR,   0x00000000,         0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTBASE,      CALI_STR,   CALI_LOGAREA_VAL_2, 0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTHW,        CALI_STR,   0x00000001,         0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTASX,       CALI_STR,   0x00000000,         0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTASY,       CALI_STR,   0x00000000,         0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTAEX,       CALI_STR,   0x00000000,         0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTAEY,       CALI_STR,   CALI_HOSTAEY_VAL,   0x00000000, 0},
};

const static shdisp_clmrRegSetting_t SRAM_dump_access_setting[] = {
    {SHDISP_CLMR_REG_HOSTSYS,       CALI_STR,   0x00000012,         0x00000000, 0},
    {SHDISP_CLMR_REG_ARMSRAMCTL,    CALI_STR,   0x00000001,         0x00000000, 0},
    {SHDISP_CLMR_REG_ARMHOSTSYS,    CALI_STR,   0x00000002,         0x00000000, 0},
    {SHDISP_CLMR_REG_ARMCONTCLKEN,  CALI_STR,   0x00000001,         0x00000000, 0},
    {SHDISP_CLMR_REG_ARMSLPCLKEN,   CALI_STR,   0x00000001,         0x00000000, 0},
    {SHDISP_CLMR_REG_ARMSRAMADR,    CALI_STR,   0x00000000,         0x00000000, 0}
};

const static shdisp_clmrRegSetting_t SRAM_dump_arm_setting[] = {
    {SHDISP_CLMR_REG_ARMSRAMCTL,    CALI_STR,   0x00000000,         0x00000000, 0},
    {SHDISP_CLMR_REG_ARMHOSTSYS,    CALI_STR,   0x00000000,         0x00000000, 0},
    {SHDISP_CLMR_REG_ARMCONTCLKEN,  CALI_STR,   0x00000000,         0x00000000, 0},
    {SHDISP_CLMR_REG_ARMSLPCLKEN,   CALI_STR,   0x00000000,         0x00000000, 0}
};
#endif /* SHDISP_CLMR_FW_TIMEOUT_DUMP */
#if defined(CONFIG_ANDROID_ENGINEERING)
#define CALI_LOGREAD_HOSTBASE_VAL  (0x00007c68)
#define CALI_LOGREAD_HOSTEND_VAL   (0x00007CDF)
#define CALI_LOGREAD_HOSTAEY_VAL    (0x347)
const static shdisp_clmrRegSetting_t dump_fw_log_Setting[] = {
    {SHDISP_CLMR_REG_HOSTSYS,    CALI_STR,   0x00000012,                    0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTCTL,    CALI_STR,   0x00000000,                    0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTCTL,    CALI_STR,   0x00000001,                    0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTCTL,    CALI_STR,   0x00000000,                    0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTBASE,   CALI_STR,   CALI_LOGREAD_HOSTBASE_VAL,     0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTHW,     CALI_STR,   0x00000001,                    0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTASX,    CALI_STR,   0x00000000,                    0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTASY,    CALI_STR,   0x00000000,                    0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTAEX,    CALI_STR,   0x00000000,                    0x00000000, 0},
    {SHDISP_CLMR_REG_HOSTAEY,    CALI_STR,   CALI_LOGREAD_HOSTAEY_VAL,      0x00000000, 0},
};
#endif /* defined (CONFIG_ANDROID_ENGINEERING) */

const static shdisp_clmrRegSetting_t pll_ctrl_on[] = {
    {SHDISP_CLMR_REG_PLLONCTLREG,   CALI_STR,   0x00000001,  0x00000000, 1 * 1000},
};

const static shdisp_clmrRegSetting_t pll_ctrl_off[] = {
    {SHDISP_CLMR_REG_PLLONCTLREG,   CALI_STR,   0x00000000,  0x00000000, 0},
};

const static shdisp_clmrRegSetting_t intm_set[] = {
    {SHDISP_CLMR_REG_INTM,          CALI_OR,    0x00020000,  0x00000000, 0},
    {SHDISP_CLMR_REG_INTM,          CALI_AND,   0x00000000, ~0x00020000, 0},
};

const static shdisp_clmrRegSetting_t setint2_set[] = {
    {SHDISP_CLMR_REG_SETINT2,       CALI_STR,   0x00000001,  0x00000000, 0},
};

#if defined(CONFIG_SHDISP_PANEL_GEMINI)
const static shdisp_clmrRegSetting_t mipi_skew_setting[] = {
    {SHDISP_CLMR_REG_TXBUFCTL2,     CALI_RMW,  0x00000010,  ~0x000000FF, 0}
};
#endif  /* CONFIG_SHDISP_PANEL_GEMINI */

/*---------------------------------------------------------------------------*/
/* FUNCTIONS                                                                 */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_init                                                 */
/*---------------------------------------------------------------------------*/
int shdisp_clmr_api_init(struct shdisp_kernel_context *shdisp_kerl_ctx)
{
    int rc = SHDISP_RESULT_SUCCESS;
    struct shdisp_clmr_ctrl_t *clmr_ctrl;

    SHDISP_TRACE("in\n");

    shdisp_clmr_register_driver();

    gArm_fw = arm_fw;
    gArm_fw_size = arm_fw_size;
    gArm_fw_base = arm_fw_base;
    gArm_fw_chg_flg = 0;

    clmr_ctrl = &shdisp_clmr_ctrl;

    init_completion(&clmr_ctrl->fw_boot_comp);
    init_completion(&clmr_ctrl->fw_cmd_comp);
    init_completion(&clmr_ctrl->eDramPtr_rst_comp);
    clmr_ctrl->workqueue = create_singlethread_workqueue("shdisp_clmr_queue");
    if (!clmr_ctrl->workqueue) {
        SHDISP_DEBUG("shdisp_clmr_workqueue create failed.\n" );
        rc = SHDISP_RESULT_FAILURE;
        goto workq_create_error;
    }
    INIT_WORK(&clmr_ctrl->work, shdisp_workqueue_handler_clmr);
    sema_init(&clmr_ctrl->wq_sem, 1);
    sema_init(&clmr_ctrl->fw_boot_sem, 1);
    sema_init(&clmr_ctrl->fw_cmd_sem, 1);
    sema_init(&clmr_ctrl->eDramPtr_rst_sem, 1);
    clmr_ctrl->fw_boot_excute = 0;
    clmr_ctrl->fw_cmd_excute = 0;
    clmr_ctrl->eDramPtr_rst_excute = 0;

    spin_lock_init(&clmr_ctrl->spin_lock);
    wake_lock_init(&clmr_ctrl->wake_lock,
                    WAKE_LOCK_SUSPEND, "clmr_wake_lock");

    rc = shdisp_clmr_regulator_init();
    if (rc != 0) {
        SHDISP_ERR("shdisp_clmr_regulator_init() failed. rc = %d\n", rc);
        rc = SHDISP_RESULT_FAILURE;
        goto request_regulator_init_error;
    }

    rc = shdisp_clmr_request_irq();
    if (rc != 0) {
        SHDISP_ERR("shdisp_clmr_request_irq() failed. rc = %d\n", rc);
        rc = SHDISP_RESULT_FAILURE;
        goto request_irq_error;
    }

#ifndef SHDISP_NOT_SUPPORT_NO_OS
    if (shdisp_pm_is_clmr_on() == SHDISP_DEV_STATE_ON) {
        shdisp_clmr_enable_irq();
        shdisp_clmr_regulator_on();
        rc = shdisp_SYS_Host_gpio_request(SHDISP_GPIO_NUM_CLMR_RESET);
        if (rc != 0) {
            SHDISP_ERR("shdisp_clmr_api_init gpio_request error.\n");
            goto request_gpio_error;
        }
    }
#endif

    memcpy(&(clmr_als_adjust[0]), &(shdisp_kerl_ctx->photo_sensor_adj.als_adjust[0]),
                                  sizeof(struct shdisp_als_adjust));
    memcpy(&(clmr_als_adjust[1]), &(shdisp_kerl_ctx->photo_sensor_adj.als_adjust[1]),
                                  sizeof(struct shdisp_als_adjust));

    return rc;

#ifndef SHDISP_NOT_SUPPORT_NO_OS
request_gpio_error:
#endif
request_irq_error:
request_regulator_init_error:
    destroy_workqueue(clmr_ctrl->workqueue);
    clmr_ctrl->workqueue = NULL;
workq_create_error:

    return rc;
}


#ifndef SHDISP_NOT_SUPPORT_NO_OS

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_regulator_on                                         */
/*---------------------------------------------------------------------------*/
int shdisp_clmr_api_regulator_on(void)
{
    int rc = SHDISP_RESULT_SUCCESS;

    rc = shdisp_clmr_regulator_on();
    return rc;
}
#endif  /* SHDISP_NOT_SUPPORT_NO_OS */

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_exit                                                 */
/*---------------------------------------------------------------------------*/
void shdisp_clmr_api_exit(void)
{
    int rc = SHDISP_RESULT_SUCCESS;
    struct shdisp_clmr_ctrl_t *clmr_ctrl;

    SHDISP_TRACE("in\n");

    clmr_ctrl = &shdisp_clmr_ctrl;

    wake_lock_destroy(&clmr_ctrl->wake_lock);


    rc = shdisp_clmr_config_reg(clmr_ctrl->clmr_vreg,
                    ARRAY_SIZE(apq8064_clmr_vreg),
                    clmr_ctrl->clmr_regu, SHDISP_CLMR_DISABLE);
    if (rc != 0) {
        SHDISP_ERR("shdisp_clmr_config_reg() failed. rc = %d\n", rc);
    }
    kfree(clmr_ctrl->clmr_regu);
    clmr_ctrl->clmr_regu = NULL;

    if (clmr_ctrl->workqueue) {
        flush_workqueue(clmr_ctrl->workqueue);
        destroy_workqueue(clmr_ctrl->workqueue);
        clmr_ctrl->workqueue = NULL;
    }

    free_irq(shdisp_clmr_irq, 0);
}


/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_power_on_err_info_set                                */
/*---------------------------------------------------------------------------*/
static void shdisp_clmr_api_power_on_err_info_set(int *err_info, int store_val, int store_pos)
{

}


/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_power_on                                             */
/*---------------------------------------------------------------------------*/
int shdisp_clmr_api_power_on(void)
{
    int rc = 0;
    int count = 0, count_max = 16;
    int err_info1 = 0, err_info2 = 0;
#ifdef SHDISP_RESET_LOG
    struct shdisp_dbg_error_code err_code;
    int err_code_reset;
#endif /* SHDISP_RESET_LOG */

#if defined(CONFIG_ANDROID_ENGINEERING)
    if (shdisp_dbg_get_fail_retry_flg() & SHDISP_DBG_FAIL_RETRY_OFF_CLMR_WAKE_ON) {
        count_max = 1;
    }
#endif /* CONFIG_ANDROID_ENGINEERING */
    SHDISP_TRACE("in\n");
    SHDISP_DEBUG("count_max=%d\n", count_max);
    for (count = 0; count < count_max; count++) {
        rc = shdisp_clmr_power_on();
        if (rc != SHDISP_CLMR_PWRON_RESULT_SUCCESS) {
            if (count < count_max / 2) {
                shdisp_clmr_api_power_on_err_info_set( &err_info1, rc, count );
            } else {
                shdisp_clmr_api_power_on_err_info_set( &err_info2, rc, count - (count_max / 2) );
            }
            if (count < (count_max - 1)) {
                SHDISP_WARN("clmr power on retry(%d)\n", count);
                shdisp_SYS_delay_us(5 * 1000);
            } else {
                SHDISP_ERR("clmr power on retry over.\n");
#ifdef SHDISP_RESET_LOG
                err_code.mode = SHDISP_DBG_MODE_LINUX;
                err_code.type = SHDISP_DBG_TYPE_CLMR_HW;
                err_code.code = SHDISP_DBG_CODE_RETRY_OVER;
                err_code.subcode = SHDISP_DBG_SUBCODE_DEVCODE;
                err_code_reset = 0;
#if defined(CONFIG_ANDROID_ENGINEERING)
                if (shdisp_dbg_api_get_reset_flg() & SHDISP_DBG_RESET_CLMR) {
                    err_code_reset = 1;
                }
#endif /* defined (CONFIG_ANDROID_ENGINEERING) */
                shdisp_dbg_api_err_output(&err_code, err_code_reset);
#endif /* SHDISP_RESET_LOG */
                goto lcdc_devcheck_error;
            }
        } else {
            break;
        }
    }


    rc = shdisp_clmr_clock_setting();
    if (rc != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("clock_setting failed.\n");
        rc = SHDISP_CLMR_PWRON_RESULT_PLL_ERR;
        goto clock_setting_error;
    }

    rc = shdisp_clmr_boot_fw();
    if (rc != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("fw_boot failed.\n");
        rc = SHDISP_CLMR_PWRON_RESULT_BOOTFW_ERR;
        goto fw_boot_error;
    }
    SHDISP_TRACE("out\n");

    return SHDISP_RESULT_SUCCESS;

fw_boot_error:
clock_setting_error:
    if (count < count_max / 2) {
        shdisp_clmr_api_power_on_err_info_set( &err_info1, rc, count );
    } else {
        shdisp_clmr_api_power_on_err_info_set( &err_info2, rc, count - (count_max / 2) );
    }
#ifndef SHDISP_NOT_SUPPORT_PLLONCTL_REG
    shdisp_api_set_pll_on_ctl_reg(SHDISP_CLMR_PLL_FORCE_OFF);
    shdisp_api_set_pll_on_ctl_count(0);
#else
    SHDISP_DEBUG("PLLONCTL OFF\n");
    shdisp_clmr_regSet(&pll_ctrl_off[0]);
#endif
    shdisp_clmr_gpio_reset_ctrl(SHDISP_GPIO_CTL_LOW);
    shdisp_clmr_disable_irq();
    shdisp_clmr_gpio_free();
    shdisp_clmr_regulator_off();
lcdc_devcheck_error:
    return SHDISP_RESULT_FAILURE;
}


/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_power_on                                                 */
/*---------------------------------------------------------------------------*/
static int shdisp_clmr_power_on(void)
{
    int rc = 0;
    int ret = SHDISP_CLMR_PWRON_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    SHDISP_PERFORMANCE("RESUME LCDC POWER-ON 0010 START\n");

    rc = shdisp_clmr_regulator_on();
    if (rc != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("clmr regulator_on failed.\n");
        return SHDISP_CLMR_PWRON_RESULT_REGULATOR_ERR;
    }

    rc = shdisp_clmr_gpio_request();
    if (rc != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("clmr gpio_request failed.\n");
        ret = SHDISP_CLMR_PWRON_RESULT_GPIO_ERR;
        goto gpio_request_error;
    }

    shdisp_clmr_gpio_reset_ctrl(SHDISP_GPIO_CTL_HIGH);

    shdisp_SYS_FWCMD_set_timeoutexception(0);

    shdisp_clmr_enable_irq();

    shdisp_clmr_set_dsctl();

    rc = shdisp_clmr_lcdc_devcheck();
    if (rc != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("clmr devchk failed.\n");
        ret = SHDISP_CLMR_PWRON_RESULT_DEVCHK_ERR;
        goto lcdc_devcheck_error;
    }
#ifndef SHDISP_NOT_SUPPORT_PLLONCTL_REG
    shdisp_api_set_pll_on_ctl_reg(SHDISP_CLMR_PLL_FORCE_ON);
#else
    SHDISP_DEBUG("PLLONCTL ON\n");
    shdisp_clmr_regSet(&pll_ctrl_on[0]);
#endif

    SHDISP_PERFORMANCE("RESUME LCDC POWER-ON 0010 END\n");

    SHDISP_TRACE("out\n");

    return SHDISP_CLMR_PWRON_RESULT_SUCCESS;

lcdc_devcheck_error:

    shdisp_clmr_gpio_reset_ctrl(SHDISP_GPIO_CTL_LOW);
    shdisp_clmr_disable_irq();
    shdisp_clmr_gpio_free();
gpio_request_error:
    shdisp_clmr_regulator_off();

    return ret;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_init_fw_lcae                                         */
/*---------------------------------------------------------------------------*/
int shdisp_clmr_api_init_fw_lcae(void)
{
    int rc = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    rc = shdisp_clmr_api_fw_bdic_set_param();
    if (rc != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("fw_bdic_set_param failed.\n");
        rc = SHDISP_RESULT_FAILURE;
    }

    SHDISP_TRACE("out\n");
    return rc;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_disp_init                                            */
/*---------------------------------------------------------------------------*/
int shdisp_clmr_api_disp_init(void)
{
    SHDISP_TRACE("in\n");

    SHDISP_PERFORMANCE("RESUME LCDC DISP-START 0010 START\n");

    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);

    shdisp_clmr_timing_setting();

    shdisp_clmr_prepro_setting();


    shdisp_clmr_mipi_dsi_rx_setting();

#if defined(CONFIG_SHDISP_PANEL_GEMINI)
    shdisp_clmr_tcon_setting();
#else
    shdisp_clmr_mipi_dsi_tx_setting();

    shdisp_clmr_mipi_dsi_tx_circuit_on();
#endif  /* CONFIG_SHDISP_PANEL_GEMINI */
    shdisp_FWCMD_safe_finishanddoKick();



    SHDISP_PERFORMANCE("RESUME LCDC DISP-START 0010 END\n");

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_custom_blk_init                                      */
/*---------------------------------------------------------------------------*/
int shdisp_clmr_api_custom_blk_init(void)
{
    SHDISP_TRACE("in\n");

    shdisp_clmr_custom_blk_startup();

    shdisp_clmr_custom_ewb_startup();

    shdisp_clmr_custom_trv_startup();


    shdisp_clmr_custom_pic_adj_startup();

    clmr_need_dbc_startup = 1;

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_custom_blk_bkl_on                                    */
/*---------------------------------------------------------------------------*/
int shdisp_clmr_api_custom_blk_bkl_on(void)
{
    SHDISP_TRACE("in\n");

    if (clmr_need_dbc_startup) {
        shdisp_clmr_custom_dbc_startup();
        clmr_need_dbc_startup = 0;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_get_pic_adj_matrix                                       */
/*---------------------------------------------------------------------------*/
static const unsigned char *shdisp_clmr_get_pic_adj_matrix(const int trv_status,
                                                           const struct shdisp_main_dbc *dbc,
                                                           const struct shdisp_main_pic_adj *pic_adj,
                                                           const unsigned short ap_type)
{
    const unsigned char *matrix;
    unsigned char dbc_acc = 0;
    unsigned char cam_pic_adj_mode = 0;

    dbc_acc += (dbc->mode == SHDISP_MAIN_DISP_DBC_MODE_DBC) ? DBC_ACC_MATRIX_DBC : DBC_ACC_MATRIX_OFF;
    dbc_acc += (dbc->auto_mode == SHDISP_MAIN_DISP_DBC_AUTO_MODE_ON) ? DBC_ACC_MATRIX_ACC : DBC_ACC_MATRIX_OFF;

    if (trv_status == SHDISP_CLMR_TRV_ON) {
        matrix = pic_adj_matrix_trv;
    } else {
        if (ap_type == SHDISP_LCDC_PIC_ADJ_AP_CAM) {
            cam_pic_adj_mode = (pic_adj->mode == SHDISP_MAIN_DISP_PIC_ADJ_MODE_00) ? 0 : 1;
            matrix = pic_adj_matrix_camera[dbc_acc][cam_pic_adj_mode];
        } else {
            matrix = pic_adj_matrix[dbc_acc][pic_adj->mode];
        }
    }
    return matrix;
}

#if defined(CONFIG_SHDISP_PANEL_GEMINI)
/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_mipi_skew_set                                        */
/*---------------------------------------------------------------------------*/
int shdisp_clmr_api_mipi_skew_set(void)
{
    SHDISP_TRACE("in\n");

    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
    shdisp_clmr_regSetwithFW(&mipi_skew_setting[0]);
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}
#endif  /* CONFIG_SHDISP_PANEL_GEMINI */

#if defined(CONFIG_SHDISP_PANEL_RYOMA) || defined(CONFIG_SHDISP_PANEL_GEMINI)
/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_set_device                                           */
/*---------------------------------------------------------------------------*/
void shdisp_clmr_api_set_device( int ctrl, unsigned char maxFR, unsigned char minFR )
{
    const unsigned char *set;

    SHDISP_TRACE("in\n");
    SHDISP_DEBUG("now status = %d", clmr_trv_info.status);

    set = shdisp_clmr_get_pic_adj_matrix(clmr_trv_info.status, &clmr_dbc, &clmr_pic_adj, clmr_ap_type);
    SHDISP_DEBUG("set = TRV(%d)\n", set[PIC_ADJ_MATRIX_TRV]);

    if (set[PIC_ADJ_MATRIX_TRV]) {
        minFR = SHDISP_PANEL_RATE_60_0;
    }
    shdisp_panel_API_request_RateCtrl(ctrl, maxFR, minFR);

    SHDISP_TRACE("out\n");
}
#endif

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_display_stop                                         */
/*---------------------------------------------------------------------------*/
void shdisp_clmr_api_display_stop(void)
{
    SHDISP_TRACE("in\n");

    SHDISP_PERFORMANCE("SUSPEND LCDC DISP-STOP 0010 START\n");


    shdisp_clmr_data_transfer_stops();

    shdisp_clmr_custom_blk_stop();

    shdisp_clmr_mipi_dsi_tx_circuit_off();

    shdisp_clmr_mipi_dsi_rx_circuit_off();

    SHDISP_PERFORMANCE("SUSPEND LCDC DISP-STOP 0010 END\n");

    SHDISP_TRACE("out\n");
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_clock_stop                                               */
/*---------------------------------------------------------------------------*/
static void shdisp_clmr_clock_stop(void)
{
    int count = 0;
    int size = ARRAY_SIZE(clock_stop);

    SHDISP_TRACE("in\n");

    SHDISP_PERFORMANCE("SUSPEND LCDC CLK-STOP 0010 START\n");

    for (count = 0; count < size; count++) {
        shdisp_clmr_regSet(&clock_stop[count]);
    }

    SHDISP_PERFORMANCE("SUSPEND LCDC CLK-STOP 0010 END\n");

    SHDISP_TRACE("out\n");
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_arm_reset                                                */
/*---------------------------------------------------------------------------*/
static void shdisp_clmr_arm_reset(void)
{
    SHDISP_TRACE("in\n");

    shdisp_clmr_regSet(&arm_reset[0]);

    SHDISP_TRACE("out\n");
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_power_off                                            */
/*---------------------------------------------------------------------------*/
int shdisp_clmr_api_power_off(void)
{
#ifndef SHDISP_NOT_SUPPORT_PLLONCTL_REG
    int count = 0;
#endif

    SHDISP_TRACE("in\n");

    SHDISP_PERFORMANCE("SUSPEND LCDC POWER-OFF 0010 START\n");

    shdisp_clmr_arm_reset();

    shdisp_clmr_pwroff_fwlog_dump();

    shdisp_clmr_clock_stop();

    gpclk_init = 0;

#ifndef SHDISP_NOT_SUPPORT_PLLONCTL_REG
    count = shdisp_api_get_pll_on_ctl_count();
    if (count >= 2) {
        SHDISP_ERR("SHDISP_CLMR_PLL_OFF Countclear \n");
        shdisp_api_set_pll_on_ctl_reg(SHDISP_CLMR_PLL_FORCE_OFF);
    } else {
        shdisp_api_set_pll_on_ctl_reg(SHDISP_CLMR_PLL_OFF);
    }
    shdisp_api_set_pll_on_ctl_count(0);
#else
    SHDISP_DEBUG("PLLONCTL OFF\n");
    shdisp_clmr_regSet(&pll_ctrl_off[0]);
#endif

    shdisp_clmr_gpio_reset_ctrl(SHDISP_GPIO_CTL_LOW);

    shdisp_clmr_disable_irq();

    shdisp_clmr_regulator_off();

    shdisp_clmr_gpio_free();

    shdisp_SYS_FWCMD_set_timeoutexception(0);

    SHDISP_PERFORMANCE("SUSPEND LCDC POWER-OFF 0010 END\n");

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_clmr_api_pll_ctrl_reg                                              */
/* ------------------------------------------------------------------------- */
void shdisp_clmr_api_pll_ctrl_reg(int ctrl)
{
#ifndef SHDISP_NOT_SUPPORT_PLLONCTL_REG
    unsigned char   count;
    int ret = SHDISP_RESULT_SUCCESS;

    count = shdisp_api_get_pll_on_ctl_count();

    if (ctrl == SHDISP_CLMR_PLL_FORCE_ON) {
        SHDISP_INFO("PLL_FORCE ON\n");
        ret = shdisp_clmr_regSet(&pll_ctrl_on[0]);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("PLLONCTL REGSET failed\n");
            return ;
        }
        count++;
        shdisp_api_set_pll_on_ctl_count(count);
    } else if (ctrl == SHDISP_CLMR_PLL_FORCE_OFF) {
        SHDISP_INFO("PLL_FORCE OFF\n");
        ret = shdisp_clmr_regSet(&pll_ctrl_off[0]);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("PLLONCTL REGSET failed\n");
            return ;
        }
        count--;
        shdisp_api_set_pll_on_ctl_count(count);
    } else if (shdisp_pm_is_clmr_on() == SHDISP_DEV_STATE_ON) {
        if (ctrl == SHDISP_CLMR_PLL_ON) {
            if (count == 0) {

                SHDISP_INFO("PLL_ON_CTL ON\n");
                ret = shdisp_clmr_regSet(&pll_ctrl_on[0]);
                if (ret != SHDISP_RESULT_SUCCESS) {
                    SHDISP_ERR("PLLONCTL REGSET failed\n");
                    return ;
                }
            }
            count++;
            shdisp_api_set_pll_on_ctl_count(count);
        } else {
            if (count == 1) {
                SHDISP_INFO("PLL_ON_CTL OFF\n");
                ret = shdisp_clmr_regSet(&pll_ctrl_off[0]);
                if (ret != SHDISP_RESULT_SUCCESS) {
                    SHDISP_ERR("PLLONCTL REGSET failed\n");
                    return ;
                }
            }
            if (count >= 1) {
                count--;
                shdisp_api_set_pll_on_ctl_count(count);
            } else {
                SHDISP_INFO("already PLL_ON_CTL OFF exit\n");
            }
        }
    } else {
    }
#else
    if (ctrl == SHDISP_CLMR_PLL_FORCE_ON) {
        SHDISP_INFO("PLL_FORCE ON \n");
        shdisp_clmr_regSet(&pll_ctrl_on[0]);
    }
#endif
    return ;
}


/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_pll_ctrl_gpio                                        */
/*---------------------------------------------------------------------------*/
int shdisp_clmr_api_pll_ctrl_gpio(int ctrl)
{
    static int pllgpiostate = 0;
    if (pllgpiostate == ctrl) {
        SHDISP_ERR("error! request = 0x%x, state = 0x%x\n", pllgpiostate, ctrl);
    }
    pllgpiostate = ctrl;

    if (ctrl) {

        shdisp_SYS_Host_gpio_request(SHDISP_GPIO_NUM_CLMR_PLLONCTL);
        shdisp_SYS_delay_us(1000);
    } else {
        shdisp_SYS_Host_gpio_free(SHDISP_GPIO_NUM_CLMR_PLLONCTL);
    }

    return SHDISP_RESULT_SUCCESS;
}


/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_gpclk_ctrl                                           */
/*---------------------------------------------------------------------------*/
void shdisp_clmr_api_gpclk_ctrl(int ctrl)
{
    int count = 0;
    int size = 0;
    shdisp_clmrRegSetting_t *gpclkCtrl;

    SHDISP_TRACE("in ctrl=%d\n", ctrl);

    if (gpclk_init == 0) {
        size = ARRAY_SIZE(gpclkInit);
        if (shdisp_FWCMD_buf_get_nokick()) {
            for (count = 0; count < size; count++) {
                shdisp_clmr_regSetwithFW((const shdisp_clmrRegSetting_t *)&gpclkInit[count]);
            }
        } else {
            for (count = 0; count < size; count++) {
                shdisp_clmr_regSet((const shdisp_clmrRegSetting_t *)&gpclkInit[count]);
            }
        }
        gpclk_init = 1;
    }

    if (SHDISP_CLMR_GPCLK_ON == ctrl) {
        gpclkCtrl = (shdisp_clmrRegSetting_t *)&gpclkOn[0];
        size = ARRAY_SIZE(gpclkOn);
    } else {
        gpclkCtrl = (shdisp_clmrRegSetting_t *)&gpclkOff[0];
        size = ARRAY_SIZE(gpclkOff);
    }

    if (shdisp_FWCMD_buf_get_nokick()) {
        for (count = 0; count < size; count++) {
            shdisp_clmr_regSetwithFW((const shdisp_clmrRegSetting_t *)&gpclkCtrl[count]);
        }
    } else {
        for (count = 0; count < size; count++) {
            shdisp_clmr_regSet((const shdisp_clmrRegSetting_t *)&gpclkCtrl[count]);
        }
    }
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_is_rate_check_mode_ctrl_on                           */
/*---------------------------------------------------------------------------*/
int shdisp_clmr_api_is_rate_check_mode_ctrl_on(void)
{
    int ret = TRUE;

    SHDISP_TRACE("in \n");

#if defined(CONFIG_SHDISP_PANEL_ANDY)
    if ((clmr_ap_type == SHDISP_LCDC_PIC_ADJ_AP_1SEG)
     || (clmr_ap_type == SHDISP_LCDC_PIC_ADJ_AP_FULLSEG)
     || (clmr_ap_type == SHDISP_LCDC_PIC_ADJ_AP_TMM)) {
        ret = FALSE;
    }
#endif /* CONFIG_SHDISP_PANEL_ANDY */

    SHDISP_TRACE("out ret=%d\n", ret)

    return ret;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_rate_check_mode_ctrl                                 */
/*---------------------------------------------------------------------------*/
void shdisp_clmr_api_rate_check_mode_ctrl(int ctrl)
{
#if !defined(CONFIG_SHDISP_PANEL_GEMINI)
    SHDISP_TRACE("in ctrl=%d\n", ctrl);

    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);

    if (ctrl == SHDISP_CLMR_RATE_CHECK_MODE_ON) {
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_RATE_CHECK_MODE, sizeof(rate_check_mode_on),
                                                                (unsigned char *)rate_check_mode_on);
    } else {
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_RATE_CHECK_MODE, sizeof(rate_check_mode_off),
                                                                (unsigned char *)rate_check_mode_off);
    }

    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);

    SHDISP_TRACE("out\n")
#endif  /* CONFIG_SHDISP_PANEL_GEMINI */
}

/*---------------------------------------------------------------------------*/
/*      sshdisp_clmr_api_vcom_tracking                                       */
/*---------------------------------------------------------------------------*/
void shdisp_clmr_api_vcom_tracking(void)
{
#if !defined(CONFIG_SHDISP_PANEL_GEMINI)
    SHDISP_TRACE("in\n");
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_VCOM_TRACKING, sizeof(vcom_tracking),
                                                                (unsigned char *)vcom_tracking);
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);

    SHDISP_TRACE("out\n")
#endif  /* CONFIG_SHDISP_PANEL_GEMINI */
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_lcdc_devcheck                                        */
/*---------------------------------------------------------------------------*/
int shdisp_clmr_api_lcdc_devcheck(void)
{
    int ret;

    SHDISP_TRACE("in\n");
    ret = shdisp_clmr_regulator_on();
    if (ret != SHDISP_RESULT_SUCCESS) {
        ret = SHDISP_RESULT_FAILURE;
        goto regulator_error;
    }

    ret = shdisp_clmr_gpio_request();
    if (ret != SHDISP_RESULT_SUCCESS) {
        shdisp_clmr_regulator_off();
        ret = SHDISP_RESULT_FAILURE;
        goto gpio_request_error;
    }

    shdisp_clmr_gpio_reset_ctrl(SHDISP_GPIO_CTL_HIGH);

    shdisp_clmr_set_dsctl();

    ret = shdisp_clmr_lcdc_devcheck();

    shdisp_clmr_gpio_reset_ctrl(SHDISP_GPIO_CTL_LOW);

    shdisp_clmr_gpio_free();

gpio_request_error:
    shdisp_clmr_regulator_off();

regulator_error:
    SHDISP_TRACE("out ret=%d\n", ret);

    return ret;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_fw_detlcdandbdic_ctrl                                */
/*---------------------------------------------------------------------------*/
void shdisp_clmr_api_fw_detlcdandbdic_ctrl(int ctrl)
{
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);

    if (ctrl) {
        shdisp_clmr_regSetwithFW(&intm_set[0]);
    } else {
        shdisp_clmr_regSetwithFW(&intm_set[1]);
    }

    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_SETINT2_0_on                                         */
/*---------------------------------------------------------------------------*/
void shdisp_clmr_api_SETINT2_0_on(void)
{
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
    shdisp_clmr_regSetwithFW(&setint2_set[0]);
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_tx_stop                                              */
/*---------------------------------------------------------------------------*/
void shdisp_clmr_api_tx_stop(void)
{
    int isdoKick;

    isdoKick = !shdisp_FWCMD_buf_get_nokick();

    if (isdoKick) {
        shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
    }
    shdisp_clmr_data_transfer_stops();

    shdisp_clmr_mipi_dsi_tx_circuit_off();

    if (isdoKick) {
        shdisp_FWCMD_safe_finishanddoKick();
        shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
    }
}


/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_tx_setttingandon                                     */
/*---------------------------------------------------------------------------*/
void shdisp_clmr_api_tx_setttingandon(void)
{
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);

    shdisp_clmr_mipi_dsi_tx_setting();

    shdisp_clmr_mipi_dsi_tx_circuit_on();

    shdisp_FWCMD_safe_finishanddoKick();
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_mipi_dsi_tx_circuit_on                               */
/*---------------------------------------------------------------------------*/
void shdisp_clmr_api_mipi_dsi_tx_circuit_on(void)
{
    shdisp_clmr_mipi_dsi_tx_setting();

    shdisp_clmr_mipi_dsi_tx_circuit_on();
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_mipi_dsi_tx_circuit_off                              */
/*---------------------------------------------------------------------------*/
void shdisp_clmr_api_mipi_dsi_tx_circuit_off(void)
{
    shdisp_clmr_mipi_dsi_tx_circuit_off();
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_api_check_sensor_param                                        */
/* ------------------------------------------------------------------------- */

void shdisp_clmr_api_check_sensor_param(struct shdisp_photo_sensor_adj *adj_in,
                                        struct shdisp_photo_sensor_adj *adj_out)
{
    struct shdisp_photo_sensor_adj tmp_adj;
    int err_flg = 0;
    unsigned long chksum;

    SHDISP_TRACE("in\n")
    memcpy(&tmp_adj, adj_in, sizeof(struct shdisp_photo_sensor_adj));

    SHDISP_INFO(" ---before---\n");
    SHDISP_INFO(" chksum        = 0x%08x\n", (unsigned int)tmp_adj.chksum);
    SHDISP_INFO(" status        = 0x%02x\n", tmp_adj.status);
    SHDISP_INFO(" als_adj0      = 0x%04x\n", (unsigned short)tmp_adj.als_adjust[0].als_adj0);
    SHDISP_INFO(" als_adj1      = 0x%04x\n", (unsigned short)tmp_adj.als_adjust[0].als_adj1);
    SHDISP_INFO(" als_shift     = 0x%02x\n", tmp_adj.als_adjust[0].als_shift);
    SHDISP_INFO(" clear_offset  = 0x%02x\n", tmp_adj.als_adjust[0].clear_offset);
    SHDISP_INFO(" ir_offset     = 0x%02x\n", tmp_adj.als_adjust[0].ir_offset);
    SHDISP_INFO(" als_adj0      = 0x%04x\n", (unsigned short)tmp_adj.als_adjust[1].als_adj0);
    SHDISP_INFO(" als_adj1      = 0x%04x\n", (unsigned short)tmp_adj.als_adjust[1].als_adj1);
    SHDISP_INFO(" als_shift     = 0x%02x\n", tmp_adj.als_adjust[1].als_shift);
    SHDISP_INFO(" clear_offset  = 0x%02x\n", tmp_adj.als_adjust[1].clear_offset);
    SHDISP_INFO(" ir_offset     = 0x%02x\n", tmp_adj.als_adjust[1].ir_offset);
    SHDISP_INFO(" key_backlight = 0x%02x\n", tmp_adj.key_backlight);

    if (tmp_adj.status != 0x90) {
        err_flg = 1;
        SHDISP_DEBUG(": status check error.\n");
    } else if (tmp_adj.als_adjust[0].als_shift > 0x1F) {
        err_flg = 2;
        SHDISP_ERR(": als_shift check error.\n");
    } else if (tmp_adj.als_adjust[1].als_shift > 0x1F) {
        err_flg = 3;
        SHDISP_ERR(": als_shift check error.\n");
    } else {
        chksum = (unsigned long)tmp_adj.status
                  + (unsigned long)tmp_adj.key_backlight
                  + tmp_adj.als_adjust[0].als_adj0
                  + tmp_adj.als_adjust[0].als_adj1
                  + (unsigned long)tmp_adj.als_adjust[0].als_shift
                  + (unsigned long)tmp_adj.als_adjust[0].clear_offset
                  + (unsigned long)tmp_adj.als_adjust[0].ir_offset
                  + tmp_adj.als_adjust[1].als_adj0
                  + tmp_adj.als_adjust[1].als_adj1
                  + (unsigned long)tmp_adj.als_adjust[1].als_shift
                  + (unsigned long)tmp_adj.als_adjust[1].clear_offset
                  + (unsigned long)tmp_adj.als_adjust[1].ir_offset;
        if (tmp_adj.chksum != chksum) {
            err_flg = 9;
            SHDISP_ERR(": chksum check error.\n");
            SHDISP_ERR(" chksum = 0x%08x\n", (unsigned int)tmp_adj.chksum);
            SHDISP_ERR(" result = 0x%08x\n", (unsigned int)chksum);
        }
    }
    if (err_flg != 0) {
        shdisp_bdic_api_set_default_sensor_param(&tmp_adj);
        tmp_adj.status = (unsigned char)err_flg;
    }
    memcpy(adj_out, &tmp_adj, sizeof(struct shdisp_photo_sensor_adj));


    SHDISP_INFO("---after---\n");
    SHDISP_INFO(" chksum        = 0x%08x\n", (unsigned int)tmp_adj.chksum);
    SHDISP_INFO(" status        = 0x%02x\n", tmp_adj.status);
    SHDISP_INFO(" als_adj0      = 0x%04x\n", (unsigned short)tmp_adj.als_adjust[0].als_adj0);
    SHDISP_INFO(" als_adj1      = 0x%04x\n", (unsigned short)tmp_adj.als_adjust[0].als_adj1);
    SHDISP_INFO(" als_shift     = 0x%02x\n", tmp_adj.als_adjust[0].als_shift);
    SHDISP_INFO(" clear_offset  = 0x%02x\n", tmp_adj.als_adjust[0].clear_offset);
    SHDISP_INFO(" ir_offset     = 0x%02x\n", tmp_adj.als_adjust[0].ir_offset);
    SHDISP_INFO(" als_adj0      = 0x%04x\n", (unsigned short)tmp_adj.als_adjust[1].als_adj0);
    SHDISP_INFO(" als_adj1      = 0x%04x\n", (unsigned short)tmp_adj.als_adjust[1].als_adj1);
    SHDISP_INFO(" als_shift     = 0x%02x\n", tmp_adj.als_adjust[1].als_shift);
    SHDISP_INFO(" clear_offset  = 0x%02x\n", tmp_adj.als_adjust[1].clear_offset);
    SHDISP_INFO(" ir_offset     = 0x%02x\n", tmp_adj.als_adjust[1].ir_offset);
    SHDISP_INFO(" key_backlight = 0x%02x\n", tmp_adj.key_backlight);
    SHDISP_TRACE("out\n")
    return;
}

#ifdef SHDISP_USE_LEDC
static shdisp_clmrRegSetting_t const vsn_power_on[] = {
    {SHDISP_CLMR_REG_GIO01,         CALI_OR,    0x00000001,  0x00000000, 10 * 1000}
};

static shdisp_clmrRegSetting_t const vsn_power_off[] = {
    {SHDISP_CLMR_REG_GIO01,         CALI_AND,   0x00000000, ~0x00000001, 1000}
};

static shdisp_clmrRegSetting_t const vsp_power_on[] = {
    {SHDISP_CLMR_REG_GIO12,         CALI_OR,    0x00000001,  0x00000000, 10 * 1000}
};

static shdisp_clmrRegSetting_t const vsp_power_off[] = {
    {SHDISP_CLMR_REG_GIO12,         CALI_AND,   0x00000000, ~0x00000001, 1000}
};

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_lcd_vsp_power_on                                     */
/*---------------------------------------------------------------------------*/
void shdisp_clmr_api_lcd_vsp_power_on(void)
{
    int i = 0;

    SHDISP_TRACE("in\n");
    for (i = 0; i < ARRAY_SIZE(vsp_power_on); i++) {
        shdisp_clmr_regSetwithFW(&vsp_power_on[i]);
    }
    SHDISP_TRACE("out\n");
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_lcd_vsp_power_off                                    */
/*---------------------------------------------------------------------------*/
void shdisp_clmr_api_lcd_vsp_power_off(void)
{
    int i = 0;

    SHDISP_TRACE("in\n");
    for (i = 0; i < ARRAY_SIZE(vsp_power_off); i++) {
        shdisp_clmr_regSetwithFW(&vsp_power_off[i]);
    }
    SHDISP_TRACE("out\n");
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_lcd_vsn_power_on                                     */
/*---------------------------------------------------------------------------*/
void shdisp_clmr_api_lcd_vsn_power_on(void)
{
    int i = 0;

    SHDISP_TRACE("in\n");
    for (i = 0; i < ARRAY_SIZE(vsn_power_on); i++) {
        shdisp_clmr_regSetwithFW(&vsn_power_on[i]);
    }
    SHDISP_TRACE("out\n");
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_lcd_vsn_power_off                                    */
/*---------------------------------------------------------------------------*/
void shdisp_clmr_api_lcd_vsn_power_off(void)
{
    int i = 0;

    SHDISP_TRACE("in\n");
    for (i = 0; i < ARRAY_SIZE(vsn_power_off); i++) {
        shdisp_clmr_regSetwithFW(&vsn_power_off[i]);
    }
    SHDISP_TRACE("out\n");
}
#endif  /* SHDISP_USE_LEDC */

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_api_eDramPtr_rst_start                                        */
/* ------------------------------------------------------------------------- */
void shdisp_clmr_api_eDramPtr_rst_start(void)
{
    shdisp_clmr_eDramPtr_rst_start();
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_lcdc_devcheck                                            */
/*---------------------------------------------------------------------------*/
static int shdisp_clmr_lcdc_devcheck(void)
{
    int ret;
    int size = sizeof(char) * 4;
    unsigned long regVal;
    union data_t {
        unsigned long lDat;
        unsigned char cDat[4];
    } sData;

    SHDISP_TRACE("in\n");
    shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_REG_DEVCODE, NULL, 0, &sData.cDat[0], size);

    ret = SHDISP_RESULT_SUCCESS;
    regVal = ntohl(sData.lDat);
    if (SHDISP_CLMR_DEVCODE_VALUE != regVal) {
        SHDISP_ERR("[%d] error. devcode = 0x%04x\n", __LINE__, (int)regVal);
        ret = SHDISP_RESULT_FAILURE;
    }
    SHDISP_TRACE("out DevCode=0x%04x ret=%d\n", (int)regVal, ret);

    return ret;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_gpio_reset_ctrl                                          */
/*---------------------------------------------------------------------------*/
static int shdisp_clmr_gpio_reset_ctrl(int ctrl)
{
#if USE_LINUX
    int ret = 0;
    if (ctrl == SHDISP_GPIO_CTL_HIGH) {
        shdisp_SYS_delay_us(5 * 1000);
    }
    if (ctrl == SHDISP_GPIO_CTL_HIGH) {
        ret = shdisp_SYS_Host_gpio_request(SHDISP_GPIO_NUM_CLMR_RESET);
        if (ret != 0) {
            SHDISP_ERR("shdisp_clmr_gpio_reset_ctrl gpio_request error.\n");
            return SHDISP_RESULT_FAILURE;
        }
    } else {
        shdisp_SYS_Host_gpio_free(SHDISP_GPIO_NUM_CLMR_RESET);
    }
    if (ctrl == SHDISP_GPIO_CTL_HIGH) {
        shdisp_SYS_delay_us(1 * 1000);
    }
#else
    if (ctrl == SHDISP_GPIO_CTL_HIGH) {
        shdisp_SYS_delay_us(5 * 1000);
    }
    shdisp_SYS_set_Host_gpio(SHDISP_GPIO_NUM_CLMR_RESET, ctrl);

    if (ctrl == SHDISP_GPIO_CTL_HIGH) {
        shdisp_SYS_delay_us(1 * 1000);
    }
#endif
    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}



/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_regulator_init                                           */
/*---------------------------------------------------------------------------*/
static int shdisp_clmr_regulator_init(void)
{
    int rc = SHDISP_RESULT_SUCCESS;
#ifdef USE_LINUX
    struct shdisp_clmr_ctrl_t *clmr_ctrl;

    SHDISP_TRACE("in\n");

    clmr_ctrl = &shdisp_clmr_ctrl;
    clmr_ctrl->clmr_vreg = apq8064_clmr_vreg;
    clmr_ctrl->clmr_regu = kzalloc(sizeof(struct regulator *)
            * ARRAY_SIZE(apq8064_clmr_vreg), GFP_KERNEL);
    if (NULL != clmr_ctrl->clmr_regu) {
        rc = shdisp_clmr_config_reg(clmr_ctrl->clmr_vreg,
                    ARRAY_SIZE(apq8064_clmr_vreg),
                    clmr_ctrl->clmr_regu, SHDISP_CLMR_ENABLE);
        if (rc != 0) {
            SHDISP_ERR("shdisp_clmr_config_reg() failed. rc = %d\n", rc);
            rc = SHDISP_RESULT_FAILURE;
            goto config_reg_error;
        }
    } else {
        SHDISP_ERR("get memory error.\n");
        rc = -SHDISP_RESULT_FAILURE;
        goto get_memory_error;
    }
    SHDISP_TRACE("out\n");
    return rc;

config_reg_error:
    kfree(clmr_ctrl->clmr_regu);
    clmr_ctrl->clmr_regu = NULL;
get_memory_error:
    clmr_ctrl->clmr_vreg = NULL;
#endif /* USE_LINUX */
    return rc;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_regulator_on                                             */
/*---------------------------------------------------------------------------*/
static int shdisp_clmr_regulator_on(void)
{
    int rc;
    struct shdisp_clmr_ctrl_t *clmr_ctrl;

    SHDISP_TRACE("in\n");

    clmr_ctrl = &shdisp_clmr_ctrl;
    rc = shdisp_clmr_enable_reg(clmr_ctrl->clmr_vreg,
                        ARRAY_SIZE(apq8064_clmr_vreg),
                        clmr_ctrl->clmr_regu, SHDISP_CLMR_ENABLE);
    if (rc != 0) {
        SHDISP_ERR("shdisp_clmr_enable_reg() failed. rc = %d\n", rc);
        return SHDISP_RESULT_FAILURE;
    }
    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;


}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_regulator_off                                            */
/*---------------------------------------------------------------------------*/
static int shdisp_clmr_regulator_off(void)
{
    int rc;
    struct shdisp_clmr_ctrl_t *clmr_ctrl;

    SHDISP_TRACE("in\n");

    clmr_ctrl = &shdisp_clmr_ctrl;
    rc = shdisp_clmr_enable_reg(clmr_ctrl->clmr_vreg,
                        ARRAY_SIZE(apq8064_clmr_vreg),
                        clmr_ctrl->clmr_regu, SHDISP_CLMR_DISABLE);
    if (rc != 0) {
        SHDISP_ERR("shdisp_clmr_enable_reg() failed. rc = %d\n", rc);
    }


    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_gpio_request                                             */
/*---------------------------------------------------------------------------*/
static int shdisp_clmr_gpio_request(void)
{
    int rc;

    SHDISP_TRACE("in\n");

    rc = shdisp_SYS_clmr_spi_gpio_init();
    if (rc != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("failed. rc = %d\n", rc);
        return rc;
    }

    SHDISP_TRACE("out\n");

    return SHDISP_RESULT_SUCCESS;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_gpio_free                                                */
/*---------------------------------------------------------------------------*/
static int shdisp_clmr_gpio_free(void)
{
    return SHDISP_RESULT_SUCCESS;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_boot_fw                                                  */
/*---------------------------------------------------------------------------*/
static int shdisp_clmr_boot_fw(void)
{
    int rc = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    rc = shdisp_clmr_fw_download();
    if (rc != SHDISP_RESULT_SUCCESS) {
        return rc;
    }

    shdisp_clmr_arm_init();

    rc = shdisp_clmr_arm_sram2fw();
    if (rc == SHDISP_RESULT_SUCCESS) {
        shdisp_clmr_arm_boot();

        rc = shdisp_clmr_wait4fw_boot_comp();
    }

    SHDISP_TRACE("out\n");

    return rc;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_api_fw_bdic_set_param                                         */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_api_fw_bdic_set_param(void)
{
    int ret;

    SHDISP_TRACE("in\n");
    shdisp_SYS_bdic_i2c_set_api(SHDISP_CLMR_FWCMD_APINO_BKL);
    ret = shdisp_clmr_pd_fw_bdic_set_param();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("ret=%d\n", ret);
        return SHDISP_RESULT_FAILURE;
    }
    ret = shdisp_SYS_bdic_i2c_doKick_if_exist();
    SHDISP_TRACE("out\n");
    return ret;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_pd_fw_bdic_set_param                                          */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_pd_fw_bdic_set_param(void)
{
    int ret;
    unsigned char *parama;
    unsigned char *paramb;

    SHDISP_TRACE("in\n");

    shdisp_clmr_ld_set_sensor_param(&(shdisp_cal_fw_lc_parama[12]));

    shdisp_cal_fw_lc_paramb[1] = shdisp_bdic_api_get_LC_MLED01();

    parama = (unsigned char *)shdisp_cal_fw_lc_parama;

    ret = shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_LIGHTCTL_WRITE, 59, parama + 1);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("ret=%d\n", ret);
        return SHDISP_RESULT_FAILURE;
    }

    paramb = (unsigned char *)shdisp_cal_fw_lc_paramb;
    ret = shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_LIGHTCTL_WRITE, 25, paramb + 1);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("ret=%d\n", ret);
        return SHDISP_RESULT_FAILURE;
    }
    SHDISP_TRACE("out ret=%d\n", ret);
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_ld_set_sensor_param                                           */
/* ------------------------------------------------------------------------- */
static void shdisp_clmr_ld_set_sensor_param(unsigned short *sensor_lc_parama)
{
    SHDISP_TRACE("in\n")
    *(sensor_lc_parama + 0) = clmr_als_adjust[0].als_adj0;
    *(sensor_lc_parama + 1) = clmr_als_adjust[0].als_adj1;
    *(sensor_lc_parama + 2) = clmr_als_adjust[0].als_shift;
    *(sensor_lc_parama + 3) = ((clmr_als_adjust[0].ir_offset << 8) | clmr_als_adjust[0].clear_offset);

    *(sensor_lc_parama + 4) = clmr_als_adjust[1].als_adj0;
    *(sensor_lc_parama + 5) = clmr_als_adjust[1].als_adj1;
    *(sensor_lc_parama + 6) = clmr_als_adjust[1].als_shift;
    *(sensor_lc_parama + 7) = ((clmr_als_adjust[1].ir_offset << 8) | clmr_als_adjust[1].clear_offset);
    SHDISP_TRACE("out\n");
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_clock_setting                                            */
/*---------------------------------------------------------------------------*/
static int shdisp_clmr_clock_setting(void)
{
#define CLMR_RETRY 100
    int rc = SHDISP_RESULT_SUCCESS;
    int count = 0;
    int size = ARRAY_SIZE(clock_setting1);
    unsigned char buf[4] = {0};

    SHDISP_TRACE("in\n");

    SHDISP_PERFORMANCE("RESUME LCDC CLK-START 0010 START\n");

    for (count = 0; count < size; count++) {
        shdisp_clmr_regSet(&clock_setting1[count]);
    }

    for (count = 0; count < CLMR_RETRY; count++) {
        shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_REG_PLLSTAT,
                                        NULL, 0, buf, sizeof(buf));
        SHDISP_DEBUG("count=%d PLL1LD=%d\n",
                                                  count, (buf[3] & 0x02));
        if ((buf[3] & 0x02) == 0x02) {
            break;
        }
        shdisp_SYS_delay_us(500);
    }

    if (count == CLMR_RETRY) {
        SHDISP_ERR("PLL_LOCK timeout!!\n");
        return SHDISP_RESULT_FAILURE;
    }

    size = ARRAY_SIZE(clock_setting2);
    for (count = 0; count < size; count++) {
        shdisp_clmr_regSet(&clock_setting2[count]);
    }

    SHDISP_PERFORMANCE("RESUME LCDC CLK-START 0010 END\n");

    SHDISP_TRACE("out\n");
    return rc;
}


/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_arm_init                                                 */
/*---------------------------------------------------------------------------*/
static void shdisp_clmr_arm_init(void)
{
    int count = 0;
    int size;

    SHDISP_TRACE("in\n");

    size = ARRAY_SIZE(arm_init);
    for (count = 0; count < size; count++) {
        shdisp_clmr_regSet(&arm_init[count]);
    }

    SHDISP_TRACE("out\n");
}


/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_fw_download                                              */
/*---------------------------------------------------------------------------*/
static int shdisp_clmr_fw_download(void)
{
    int rc = SHDISP_RESULT_SUCCESS;
    int count = 0;
    int size = ARRAY_SIZE(fw_download);
    unsigned char buf[4] = {0};
    unsigned long *lBuf = (unsigned long *)&buf[0];

    SHDISP_TRACE("in\n");

#if !defined(CONFIG_ANDROID_ENGINEERING)
    gArm_fw = arm_fw;
    gArm_fw_size = arm_fw_size;
    gArm_fw_base = arm_fw_base;
    gArm_fw_chg_flg = 0;
#endif /* defined (CONFIG_ANDROID_ENGINEERING) */
    fw_download[2].data = gArm_fw_base;


    for (count = 0; count < size; count++) {
        shdisp_clmr_regSet(&fw_download[count]);
    }

    *lBuf = htonl((unsigned long)gArm_fw_size - 1);
    shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_REG_HOSTAEY,
                                        &buf[0], sizeof(buf), NULL, 0);

    rc = shdisp_SYS_clmr_sio_eDram_transfer(SHDISP_CLMR_EDRAM_C7,
                                        &gArm_fw[0], (int)gArm_fw_size * 16);

    SHDISP_TRACE("out\n");

    return rc;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_arm_sram2fw                                              */
/*---------------------------------------------------------------------------*/
static int shdisp_clmr_arm_sram2fw(void)
{
    int rc = SHDISP_RESULT_SUCCESS;
    int count = 0;
    int size = ARRAY_SIZE(arm_sram2fw);
    unsigned char buf[4] = {0};

    SHDISP_TRACE("in\n");

    arm_sram2fw[3].data = gArm_fw_base;
    for (count = 0; count < size; count++) {
        shdisp_clmr_regSet(&arm_sram2fw[count]);
    }

    for (count = 0; count < 100; count++) {
        shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_REG_ARMDMA,
                                        NULL, 0, &buf[0], sizeof(buf));
        if ((buf[3] & 0x01) == 0x00) {
            break;
        }
    }

    if (count == 100) {
        SHDISP_ERR("DMA_Transfer timeout!!\n");
        rc = SHDISP_RESULT_FAILURE;
    }

    SHDISP_TRACE("out\n");

    return rc;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_arm_boot                                                 */
/*---------------------------------------------------------------------------*/
static void shdisp_clmr_arm_boot(void)
{
    SHDISP_TRACE("in\n");

    shdisp_clmr_boot_start();
    shdisp_clmr_regSet(&arm_boot[0]);

    SHDISP_TRACE("out\n");
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_timing_setting                                           */
/*---------------------------------------------------------------------------*/
static void shdisp_clmr_timing_setting(void)
{

    int size = ARRAY_SIZE(timing_setting);

    SHDISP_TRACE("in\n");

    shdisp_clmr_regSet_multi(timing_setting, size);

    SHDISP_TRACE("out\n");
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_prepro_setting                                           */
/*---------------------------------------------------------------------------*/
static void shdisp_clmr_prepro_setting(void)
{

    int size = ARRAY_SIZE(prepro_setting);

    SHDISP_TRACE("in\n");

    shdisp_clmr_regSet_multi(prepro_setting, size);

    SHDISP_TRACE("out\n");
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_mipi_dsi_rx_setting                                      */
/*---------------------------------------------------------------------------*/
static void shdisp_clmr_mipi_dsi_rx_setting(void)
{

    int size = ARRAY_SIZE(mipi_dsi_rx_setting);

    SHDISP_TRACE("in\n");

    shdisp_clmr_regSet_multi(mipi_dsi_rx_setting, size);

    SHDISP_TRACE("out\n");
}
#if defined(CONFIG_SHDISP_PANEL_GEMINI)
/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_tcon_setting                                             */
/*---------------------------------------------------------------------------*/
static void shdisp_clmr_tcon_setting(void)
{
    int size = ARRAY_SIZE(tcon_setting);

    SHDISP_TRACE("in\n");

    shdisp_clmr_regSet_multi(tcon_setting, size);

    SHDISP_TRACE("out\n");
}
#endif  /* CONFIG_SHDISP_PANEL_GEMINI */

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_mipi_dsi_tx_setting                                      */
/*---------------------------------------------------------------------------*/
static void shdisp_clmr_mipi_dsi_tx_setting(void)
{

    int size = ARRAY_SIZE(mipi_dsi_tx_setting);

    SHDISP_TRACE("in\n");

    shdisp_clmr_regSet_multi(mipi_dsi_tx_setting, size);

    SHDISP_TRACE("out\n");
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_mipi_dsi_tx_circuit_on                                   */
/*---------------------------------------------------------------------------*/
static void shdisp_clmr_mipi_dsi_tx_circuit_on(void)
{

    int size = ARRAY_SIZE(mipi_dsi_tx_circuit_on);

    SHDISP_TRACE("in\n");

    shdisp_clmr_regSet_multi(mipi_dsi_tx_circuit_on, size);

    SHDISP_TRACE("out\n");
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_data_transfer_starts                                 */
/*---------------------------------------------------------------------------*/
void shdisp_clmr_api_data_transfer_starts(void)
{

    int size = ARRAY_SIZE(data_transfer_starts);

    SHDISP_TRACE("in\n");

    shdisp_clmr_regSet_multi(data_transfer_starts, size);

    SHDISP_TRACE("out\n");

}


/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_data_transfer_stops                                      */
/*---------------------------------------------------------------------------*/
static void shdisp_clmr_data_transfer_stops(void)
{
    int count = 0;
    int size = ARRAY_SIZE(data_transfer_stops);

    SHDISP_TRACE("in\n");

    for (count = 0; count < size; count++) {
        shdisp_clmr_regSetwithFW(&data_transfer_stops[count]);
    }

    SHDISP_TRACE("out\n");
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_mipi_dsi_tx_circuit_off                                  */
/*---------------------------------------------------------------------------*/
static void shdisp_clmr_mipi_dsi_tx_circuit_off(void)
{
    int count = 0;
    int size = ARRAY_SIZE(mipi_dsi_tx_circuit_off);

    SHDISP_TRACE("in\n");

    for (count = 0; count < size; count++) {
        shdisp_clmr_regSetwithFW(&mipi_dsi_tx_circuit_off[count]);
    }

    SHDISP_TRACE("out\n");
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_mipi_dsi_rx_circuit_off                                  */
/*---------------------------------------------------------------------------*/
static void shdisp_clmr_mipi_dsi_rx_circuit_off(void)
{
    int count = 0;
    int size = ARRAY_SIZE(mipi_dsi_rx_circuit_off);

    SHDISP_TRACE("in\n");

    for (count = 0; count < size; count++) {
        shdisp_clmr_regSetwithFW(&mipi_dsi_rx_circuit_off[count]);
    }

    SHDISP_TRACE("out\n");
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_custom_blk_startup                                       */
/*---------------------------------------------------------------------------*/
static void shdisp_clmr_custom_blk_startup(void)
{
    int size = ARRAY_SIZE(custom_vsp_init);

    SHDISP_TRACE("in\n");

    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_OTHER);

    shdisp_clmr_regSet_multi(custom_vsp_init, size);
    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE,
                        sizeof(shdisp_fwcmd_clmrVSP_on) / sizeof(char), (unsigned char *)(shdisp_fwcmd_clmrVSP_on) );
    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_VSPREGON, sizeof(shdisp_fwcmd_clrmVSP_funcon),
                                                     (unsigned char *)shdisp_fwcmd_clrmVSP_funcon );
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);

    SHDISP_TRACE("out\n");
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_custom_ewb_startup                                       */
/*---------------------------------------------------------------------------*/
static void shdisp_clmr_custom_ewb_startup(void)
{
    int ret;
#ifndef SHDISP_NOT_SUPPORT_EWB_2SURFACE
    struct shdisp_clmr_ewb_accu *ewb_accu_1;
#endif

    SHDISP_TRACE("in\n");

#ifndef SHDISP_NOT_SUPPORT_EWB_2SURFACE
    ewb_accu_1 = shdisp_clmr_api_get_ewb_accu(SHDISP_CLMR_EWB_LUT_NO_1);
    ret = shdisp_clmr_ewb_lut_write(ewb_accu_1, SHDISP_CLMR_EWB_LUT_NO_1);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_ewb_lut_write() Error!!!\n");
    }
#endif

    ret = shdisp_clmr_sqe_ewb_on(&clmr_ewb_accu_cross, SHDISP_CLMR_EWB_LUT_NO_0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_sqe_ewb_on Error!!!\n");
        return;
    }

    SHDISP_TRACE("out\n");
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_custom_trv_startup                                       */
/*---------------------------------------------------------------------------*/
static void shdisp_clmr_custom_trv_startup(void)
{
    int ret = SHDISP_RESULT_SUCCESS;
    const unsigned char *set;

    SHDISP_TRACE("in\n");
    SHDISP_DEBUG("now status = %d", clmr_trv_info.status);

    set = shdisp_clmr_get_pic_adj_matrix(clmr_trv_info.status, &clmr_dbc, &clmr_pic_adj, clmr_ap_type);
    SHDISP_DEBUG("set = SVCT(%d), HSV(%d), PCA(%d), CPF(%d), AE(%d), SBL(%d), SMITE(%d), TRV(%d)\n",
                     set[PIC_ADJ_MATRIX_SVCT],  set[PIC_ADJ_MATRIX_HSV], set[PIC_ADJ_MATRIX_PCA],
                     set[PIC_ADJ_MATRIX_CPF],   set[PIC_ADJ_MATRIX_AE],  set[PIC_ADJ_MATRIX_SBL],
                     set[PIC_ADJ_MATRIX_SMITE], set[PIC_ADJ_MATRIX_TRV]);

    if (set[PIC_ADJ_MATRIX_TRV]) {
        if ((clmr_trv_info.status == SHDISP_CLMR_TRV_ON) && (clmr_trv_info.data_size == 0)) {
            SHDISP_ERR("<INVALID_VALUE> TRV Data Not Set.\n");
            return;
        }

        ret = shdisp_clmr_sqe_trv_on();
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_sqe_trv_on Error!!!\n");
            return;
        }

        ret = shdisp_clmr_vsp_on_plus(SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_TRV);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_vsp_on Error!!!\n");
            return;
        }
    }

    SHDISP_TRACE("out\n");
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_custom_dbc_startup                                       */
/*---------------------------------------------------------------------------*/
static void shdisp_clmr_custom_dbc_startup(void)
{
    int ret = SHDISP_RESULT_SUCCESS;
    const unsigned char *set;
    unsigned char type = SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_OFF;

    SHDISP_TRACE("in\n");
    SHDISP_DEBUG("now mode = %d auto_mode = %d", clmr_dbc.mode, clmr_dbc.auto_mode);

    set = shdisp_clmr_get_pic_adj_matrix(clmr_trv_info.status, &clmr_dbc, &clmr_pic_adj, clmr_ap_type);
    SHDISP_DEBUG("set = SVCT(%d), HSV(%d), PCA(%d), CPF(%d), AE(%d), SBL(%d), SMITE(%d), TRV(%d)\n",
                     set[PIC_ADJ_MATRIX_SVCT],  set[PIC_ADJ_MATRIX_HSV], set[PIC_ADJ_MATRIX_PCA],
                     set[PIC_ADJ_MATRIX_CPF],   set[PIC_ADJ_MATRIX_AE],  set[PIC_ADJ_MATRIX_SBL],
                     set[PIC_ADJ_MATRIX_SMITE], set[PIC_ADJ_MATRIX_TRV]);

    if ((set[PIC_ADJ_MATRIX_SMITE])
     || (smite_matrix[set[PIC_ADJ_MATRIX_SMITE]] &&
         (sh_boot_get_bootmode() != SH_BOOT_D && sh_boot_get_bootmode() != SH_BOOT_F_F))) {
        ret = shdisp_clmr_sqe_smite_on(smite_matrix[set[PIC_ADJ_MATRIX_SMITE]], clmr_ap_type);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_sqe_smite_on Error!!!\n");
            return;
        }
    }

    if (set[PIC_ADJ_MATRIX_SBL] == SHDISP_CLMR_SBL_ACC) {
        ret = shdisp_clmr_sqe_sbl_on(set[PIC_ADJ_MATRIX_SBL], clmr_ap_type);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_sqe_sbl_on Error!!!\n");
            return;
        }
    }

    if ((set[PIC_ADJ_MATRIX_SBL] == SHDISP_CLMR_SBL_ACC)
     || (set[PIC_ADJ_MATRIX_SMITE])
     || (smite_matrix[set[PIC_ADJ_MATRIX_SMITE]] &&
         (sh_boot_get_bootmode() != SH_BOOT_D && sh_boot_get_bootmode() != SH_BOOT_F_F))) {
        if (set[PIC_ADJ_MATRIX_SBL] == SHDISP_CLMR_SBL_ACC) {
            type |= SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_SBL;

            if (set[PIC_ADJ_MATRIX_CPF] &&
                (clmr_pic_adj.mode != SHDISP_MAIN_DISP_PIC_ADJ_MODE_00) &&
                cpf_on_param[clmr_pic_adj.mode -1][clmr_ap_type][0]) {
                type |= SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_CPF;
            }
        }

        if (type != SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_OFF) {
            ret = shdisp_clmr_vsp_on_plus(type);
            if (ret != SHDISP_RESULT_SUCCESS) {
                SHDISP_ERR("shdisp_clmr_vsp_on_plus Error!!!\n");
                return;
            }
        } else {
            ret = shdisp_clmr_vsp_on();
            if (ret != SHDISP_RESULT_SUCCESS) {
                SHDISP_ERR("shdisp_clmr_vsp_on Error!!!\n");
                return;
            }
        }
    }

    SHDISP_TRACE("out\n");
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_custom_pic_adj_startup                                   */
/*---------------------------------------------------------------------------*/
static void shdisp_clmr_custom_pic_adj_startup(void)
{
    int ret = SHDISP_RESULT_SUCCESS;
    const unsigned char *set;
    unsigned char type = SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_OFF;

    SHDISP_TRACE("in\n");
    SHDISP_DEBUG("now mode = %d\n", clmr_pic_adj.mode);

    set = shdisp_clmr_get_pic_adj_matrix(clmr_trv_info.status, &clmr_dbc, &clmr_pic_adj, clmr_ap_type);
    SHDISP_DEBUG("set = SVCT(%d), HSV(%d), PCA(%d), CPF(%d), AE(%d), SBL(%d), SMITE(%d), TRV(%d)\n",
                     set[PIC_ADJ_MATRIX_SVCT],  set[PIC_ADJ_MATRIX_HSV], set[PIC_ADJ_MATRIX_PCA],
                     set[PIC_ADJ_MATRIX_CPF],   set[PIC_ADJ_MATRIX_AE],  set[PIC_ADJ_MATRIX_SBL],
                     set[PIC_ADJ_MATRIX_SMITE], set[PIC_ADJ_MATRIX_TRV]);

    ret = shdisp_clmr_sqe_pic_adj_on(&clmr_pic_adj, clmr_ap_type, set);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_sqe_pic_adj_on Error!!!\n");
        return;
    }

    if (set[PIC_ADJ_MATRIX_CPF] &&
        (clmr_pic_adj.mode != SHDISP_MAIN_DISP_PIC_ADJ_MODE_00) &&
        cpf_on_param[clmr_pic_adj.mode - 1][clmr_ap_type][0]) {
        type |= SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_CPF;
    }
    if ((set[PIC_ADJ_MATRIX_SBL] == SHDISP_CLMR_SBL_AE) && (sbl_on_param[SHDISP_CLMR_SBL_AE - 1][clmr_ap_type][0])) {
        type |= SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_SBL;
    }
    if (type != SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_OFF) {
        ret = shdisp_clmr_vsp_on_plus(type);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_vsp_on_plus Error!!!\n");
            return;
        }
    } else {
        ret = shdisp_clmr_vsp_on();
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_vsp_on Error!!!\n");
            return;
        }
    }

    SHDISP_TRACE("out\n");
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_api_set_context_ewb                                           */
/* ------------------------------------------------------------------------- */
void shdisp_clmr_api_set_context_ewb(struct shdisp_clmr_ewb *ewb_param_diffs)
{
    int i;

    SHDISP_TRACE("in\n");

    for (i = 0; i < SHDISP_CLMR_EWB_LUT_NUM; i++) {
        shdisp_clmr_api_convert_ewb_param(&ewb_param_diffs[i], &clmr_ewb_accu[i]);
    }

    memcpy(&clmr_ewb_accu_cross, clmr_ewb_accu, sizeof(clmr_ewb_accu_cross));

    SHDISP_TRACE("out\n");
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_api_convert_ewb_param                                         */
/* ------------------------------------------------------------------------- */
void shdisp_clmr_api_convert_ewb_param(struct shdisp_clmr_ewb *ewb_param_diff,
                                       struct shdisp_clmr_ewb_accu *ewb_param_accu)
{
    int idx;

    for (idx = 0; idx < SHDISP_LCDC_EWB_TBL_SIZE; idx++) {
        if (idx != 0) {
            ewb_param_accu->valR[idx] = ewb_param_accu->valR[idx - 1] + ewb_param_diff->valR[idx];
            ewb_param_accu->valG[idx] = ewb_param_accu->valG[idx - 1] + ewb_param_diff->valG[idx];
            ewb_param_accu->valB[idx] = ewb_param_accu->valB[idx - 1] + ewb_param_diff->valB[idx];
        } else {
            ewb_param_accu->valR[idx] = ewb_param_diff->valR[idx];
            ewb_param_accu->valG[idx] = ewb_param_diff->valG[idx];
            ewb_param_accu->valB[idx] = ewb_param_diff->valB[idx];
        }
    }
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_api_get_ewb_accu                                              */
/* ------------------------------------------------------------------------- */
struct shdisp_clmr_ewb_accu* shdisp_clmr_api_get_ewb_accu(unsigned char no)
{
    if (no >= SHDISP_CLMR_EWB_LUT_NUM) {
        return NULL;
    }
    return &clmr_ewb_accu[no];
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_api_set_ewb_tbl                                               */
/* ------------------------------------------------------------------------- */
int shdisp_clmr_api_set_ewb_tbl(struct shdisp_clmr_ewb_accu *ewb_accu, unsigned char no)
{
    int ret;

    SHDISP_TRACE("in\n");

    if (no >= SHDISP_CLMR_EWB_LUT_NUM) {
        SHDISP_ERR("<INVALID_VALUE> no(%d).\n", no);
        return SHDISP_RESULT_FAILURE;
    }
    if (ewb_accu == NULL) {
        SHDISP_ERR("<NULL_POINTER> val.\n");
        return SHDISP_RESULT_FAILURE;
    }

    ret = shdisp_clmr_sqe_ewb_on(ewb_accu, no);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_sqe_ewb_on Error!!!\n");
        return ret;
    }

    ret = shdisp_clmr_vsp_on();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_vsp_on Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_api_diag_set_ewb                                              */
/* ------------------------------------------------------------------------- */
int shdisp_clmr_api_diag_set_ewb(struct shdisp_diag_set_ewb *ewb)
{
    int ret;
    int count = 0;
    int size = ARRAY_SIZE(custom_ewb_set);

    SHDISP_TRACE("in\n");

    if (ewb == NULL) {
        SHDISP_ERR("<NULL_POINTER> val.\n");
        return SHDISP_RESULT_FAILURE;
    }

    custom_ewb_set[3].data  =  ewb->valB & 0x03FF;
    custom_ewb_set[3].data |= (ewb->valG & 0x03FF) << 10;
    custom_ewb_set[3].data |= (ewb->valR & 0x03FF) << 20;

    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_OTHER);
    for (count = 0; count < size; count++) {
        ret = shdisp_clmr_regSetwithFW(&custom_ewb_set[count]);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("EWB Set Error!!!\n");
            return ret;
        }
    }
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_api_diag_read_ewb                                             */
/* ------------------------------------------------------------------------- */
int shdisp_clmr_api_diag_read_ewb(struct shdisp_diag_read_ewb *rewb)
{
    int ret;
    int count;
    int size;
    unsigned char dat0[4];
    unsigned char dat1[4];

    SHDISP_TRACE("in\n");
    SHDISP_DEBUG("rewb->level = %d\n", rewb->level);

    if (rewb == NULL) {
        SHDISP_ERR("<NULL_POINTER> val.\n");
        return SHDISP_RESULT_FAILURE;
    }

    size = ARRAY_SIZE(custom_ewb_off);
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_OTHER);
    for (count = 0; count < size; count++) {
        ret = shdisp_clmr_regSetwithFW(&custom_ewb_off[count]);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("EWB OFF Error!!!\n");
            return ret;
        }
    }

    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);

    ret = shdisp_clmr_vsp_on();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_vsp_on Error!!!\n");
        return ret;
    }

    size = ARRAY_SIZE(custom_ewb_control_for_read1);
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_OTHER);
    custom_ewb_control_for_read1[2].data  = rewb->level & 0x000000FF;
    for (count = 0; count < size; count++) {
        ret = shdisp_clmr_regSetwithFW(&custom_ewb_control_for_read1[count]);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("EWB Control1 Error!!!\n");
            return ret;
        }
    }
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);

    size = sizeof(dat0);
    ret = shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_CUST_EWBWREXEC, NULL, 0, dat0, size);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("EWB read Dummy Error!!!\n");
        return ret;
    }
    memset(dat0, 0, size);
    ret = shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_CUST_EWBRDDAT0, NULL, 0, dat0, size);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("EWB read DAT0 Error!!!\n");
        return ret;
    }
    size = sizeof(dat1);
    memset(dat1, 0, size);
    ret = shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_CUST_EWBRDDAT1, NULL, 0, dat1, size);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("EWB read DAT1 Error!!!\n");
        return ret;
    }

    SHDISP_DEBUG("dat0 = %02x%02x%02x%02x\n", dat0[0], dat0[1], dat0[2], dat0[3]);
    SHDISP_DEBUG("dat1 = %02x%02x%02x%02x\n", dat1[0], dat1[1], dat1[2], dat1[3]);

    rewb->valR  =  dat0[3];
    rewb->valR |= (dat0[2] & 0x03) << 8;
    rewb->valG  =  dat1[1];
    rewb->valG |= (dat1[0] & 0x03) << 8;
    rewb->valB  =  dat1[3];
    rewb->valB |= (dat1[2] & 0x03) << 8;

    size = ARRAY_SIZE(custom_ewb_control_for_read2);
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_OTHER);
    for (count = 0; count < size; count++) {
        ret = shdisp_clmr_regSetwithFW(&custom_ewb_control_for_read2[count]);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("EWB Control2 Error!!!\n");
            return ret;
        }
    }

    size = ARRAY_SIZE(custom_ewb_on);
    for (count = 0; count < size; count++) {
        ret = shdisp_clmr_regSetwithFW(&custom_ewb_on[count]);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("EWB ON Error!!!\n");
            return ret;
        }
    }

    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);

    ret = shdisp_clmr_vsp_on();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_vsp_on Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sqe_ewb_on                                                    */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sqe_ewb_on(struct shdisp_clmr_ewb_accu *ewb_accu, unsigned char no)
{
    int ret;

    SHDISP_TRACE("in\n");

    if (no >= SHDISP_CLMR_EWB_LUT_NUM) {
        SHDISP_ERR("<INVALID_VALUE> no(%d).\n", no);
        return SHDISP_RESULT_FAILURE;
    }
    if (ewb_accu == NULL) {
        SHDISP_ERR("<NULL_POINTER> val.\n");
        return SHDISP_RESULT_FAILURE;
    }

    ret = shdisp_clmr_ewb_lut_write(ewb_accu, no);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_ewb_lut_write Error!!!\n");
        return ret;
    }

    ret = shdisp_clmr_ewb_param_set();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_ewb_param_set Error!!!\n");
        return ret;
    }

    ret = shdisp_clmr_ewb_on_off(SHDISP_CLMR_EWB_ON);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_ewb_on_off Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_lut_on_                                                       */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_lut_on_(unsigned char type, unsigned char no)
{
    unsigned char wdata[SHDISP_CLMR_FWCMD_LUT_ON_SIZE];

    SHDISP_TRACE("in type=%d, no=%d.\n", type, no);

    if (no >= SHDISP_CLMR_EWB_LUT_NUM) {
        SHDISP_ERR("<INVALID_VALUE> no(%d).\n", no);
        return SHDISP_RESULT_FAILURE;
    }

    wdata[0]  = 1 << type;
    if (type == SHDISP_CLMR_FWCMD_LUT_ON_EWB) {
        wdata[0] |= (no << 4 ) & 0xF0;
    }
    wdata[1]  = 0x00;

    SHDISP_DEBUG("wdata = %02x%02x\n", wdata[0], wdata[1]);
    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_LUT_ON, SHDISP_CLMR_FWCMD_LUT_ON_SIZE, wdata);

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_vsp_on_                                                       */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_vsp_on_(void)
{
    unsigned char wdata[SHDISP_CLMR_FWCMD_HOST_VSPREGON_SIZE];

    SHDISP_TRACE("in\n");

    wdata[0] = 0x03;
    wdata[1] = 0x00;

    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_VSPREGON, SHDISP_CLMR_FWCMD_HOST_VSPREGON_SIZE, wdata);

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_vsp_on                                                        */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_vsp_on(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_vsp_on_();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_vsp_on_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("VSP On Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_api_vsp_on                                                    */
/* ------------------------------------------------------------------------- */
void shdisp_clmr_api_vsp_on(void)
{
    SHDISP_TRACE("in\n");

    shdisp_clmr_vsp_on_();

    SHDISP_TRACE("out\n");
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_vsp_on_plus_                                                  */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_vsp_on_plus_(unsigned char type)
{
    unsigned char wdata[SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_SIZE];

    SHDISP_TRACE("in\n");

    wdata[0] = type;
    wdata[1] = 0x00;

    SHDISP_DEBUG("CPF = %d SBL = %d TRV = %d.\n", (wdata[0] & 0x01), (wdata[0] & 0x02), (wdata[0] & 0x04));
    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF, SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_SIZE, wdata);

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_vsp_on_plus                                                   */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_vsp_on_plus(unsigned char type)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in type=%d.\n", type);

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_vsp_on_plus_(type);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_vsp_on_plus_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("VSP On Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_ewb_lut_write_                                                */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_ewb_lut_write_(struct shdisp_clmr_ewb_accu *ewb_accu, unsigned char no)
{
    int ret = SHDISP_RESULT_SUCCESS;
    int count = 0;
    unsigned char *ewb_lut_data;

    SHDISP_TRACE("in no=%d.\n", no);

    clmr_wdata[0] = no;
    clmr_wdata[1] = 0x00;
    ewb_lut_data = &clmr_wdata[2];
    for (count = 0; count < SHDISP_LCDC_EWB_TBL_SIZE; count++) {
        ewb_lut_data[count * 4]      =  ewb_accu->valB[count] & 0x00FF;
        ewb_lut_data[count * 4 + 1]  = (ewb_accu->valB[count] >> 8) & 0x0003;
        ewb_lut_data[count * 4 + 1] |= (ewb_accu->valG[count] & 0x003F) << 2;
        ewb_lut_data[count * 4 + 2]  = (ewb_accu->valG[count] >> 6) & 0x000F;
        ewb_lut_data[count * 4 + 2] |= (ewb_accu->valR[count] & 0x000F) << 4;
        ewb_lut_data[count * 4 + 3]  = (ewb_accu->valR[count] >> 4) & 0x003F;
    }
    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_EWB_LUT_WRITE, SHDISP_CLMR_FWCMD_HOST_EWB_LUT_WRITE_SIZE, clmr_wdata);

    ret = shdisp_clmr_lut_on_(SHDISP_CLMR_FWCMD_LUT_ON_EWB, no);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_lut_on_ Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_ewb_lut_write                                                 */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_ewb_lut_write(struct shdisp_clmr_ewb_accu *ewb_accu, unsigned char no)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in no=%d.\n", no);

    if (no >= SHDISP_CLMR_EWB_LUT_NUM) {
        SHDISP_ERR("<INVALID_VALUE> no(%d).\n", no);
        return SHDISP_RESULT_FAILURE;
    }

    if (ewb_accu == NULL) {
        SHDISP_ERR("<NULL_POINTER> val.\n");
        return SHDISP_RESULT_FAILURE;
    }

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_ewb_lut_write_(ewb_accu, no);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_ewb_lut_write_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("EWB LUT write Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_ewb_param_set_                                                */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_ewb_param_set_(void)
{
    int ret = SHDISP_RESULT_SUCCESS;
    int size = ARRAY_SIZE(custom_ewb_start);

    SHDISP_TRACE("in\n");

    ret = shdisp_clmr_regsSetbyFW(custom_ewb_start, size);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_regsSetbyFW(custom_ewb_start) Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_ewb_param_set                                                 */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_ewb_param_set(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_ewb_param_set_();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_ewb_param_set_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("EWB Param Set Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_ewb_on_off_                                                   */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_ewb_on_off_(int on)
{
    unsigned char wdata[SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE_SIZE];

    SHDISP_TRACE("in on=%d.\n", on);

    wdata[0] =  SHDISP_CLMR_CUST_VSPCTRL1 & 0x00FF;
    wdata[1] = (SHDISP_CLMR_CUST_VSPCTRL1 & 0xFF00) >> 8;
    wdata[2] =  0x0008C000 & 0x000000FF;
    wdata[3] = (0x0008C000 & 0x0000FF00) >> 8;
    wdata[4] = (0x0008C000 & 0x00FF0000) >> 16;
    wdata[5] = (0x0008C000 & 0xFF000000) >> 24;
    wdata[6] =  ((on << 19) | (on << 14)) & 0x000000FF;
    wdata[7] = (((on << 19) | (on << 14)) & 0x0000FF00) >> 8;
    wdata[8] = (((on << 19) | (on << 14)) & 0x00FF0000) >> 16;
    wdata[9] = (((on << 19) | (on << 14)) & 0xFF000000) >> 24;

    SHDISP_DEBUG("wdata = %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
                    wdata[0], wdata[1], wdata[2], wdata[3], wdata[4],
                    wdata[5], wdata[6], wdata[7], wdata[8], wdata[9]);
    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE, SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE_SIZE, wdata);

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_ewb_on_off                                                    */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_ewb_on_off(int on)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in on=%d.\n", on);

    if (on > 0) {
        on = SHDISP_CLMR_EWB_ON;
    }

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_ewb_on_off_(on);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_ewb_on_off_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("EWB ON/OFF(%d) Error!!!\n", on);
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_custom_blk_stop                                          */
/*---------------------------------------------------------------------------*/
static void shdisp_clmr_custom_blk_stop(void)
{
    SHDISP_TRACE("in\n");
    SHDISP_TRACE("out\n");
}




/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_wait4fw_boot_comp                                        */
/*---------------------------------------------------------------------------*/
static int shdisp_clmr_wait4fw_boot_comp(void)
{
    unsigned long rc;
    struct shdisp_clmr_ctrl_t *clmr_ctrl;
    unsigned long waittime;
    int ret;
#ifdef SHDISP_RESET_LOG
    struct shdisp_dbg_error_code err_code;
#endif /* SHDISP_RESET_LOG */

    clmr_ctrl = &shdisp_clmr_ctrl;

    SHDISP_TRACE("in\n");
    SHDISP_PERFORMANCE_DEBUG("COMMON WAIT-FOR-FW-RESPONSE 0010 START\n");

    waittime = SHDISP_CLMR_WAIT4SYNC_TIMEOUT;
    rc = wait_for_completion_timeout(&clmr_ctrl->fw_boot_comp, waittime);
    if (0 == rc) {
        ret = shdisp_clmr_hint_failsafe(&clmr_ctrl->fw_boot_comp);
        if (ret == SHDISP_RESULT_SUCCESS) {
            return SHDISP_RESULT_SUCCESS;
        }
        SHDISP_ERR("timeout:[%d] rc=%ld\n", __LINE__, rc);
#ifdef SHDISP_RESET_LOG
        err_code.mode = SHDISP_DBG_MODE_LINUX;
        err_code.type = SHDISP_DBG_TYPE_CLMR_FW;
        err_code.code = SHDISP_DBG_CODE_TIMEOUT;
        err_code.subcode = SHDISP_DBG_SUBCODE_BOOT;
        shdisp_dbg_api_err_output(&err_code, 0);
#endif /* SHDISP_RESET_LOG */
        shdisp_clmr_reg_dump_logset();
        return SHDISP_RESULT_FAILURE;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_donefw_boot_comp                                         */
/*---------------------------------------------------------------------------*/
static void shdisp_clmr_donefw_boot_comp(void)
{
    struct shdisp_clmr_ctrl_t *clmr_ctrl;
    int tmp = 0;
#ifdef SHDISP_RESET_LOG
    struct shdisp_dbg_error_code err_code;
#endif /* SHDISP_RESET_LOG */

    clmr_ctrl = &shdisp_clmr_ctrl;
    SHDISP_TRACE("in\n");
    down(&clmr_ctrl->fw_boot_sem);
    if (clmr_ctrl->fw_boot_excute == 1) {
        tmp = 1;
    }
    clmr_ctrl->fw_boot_excute = 0;
    up(&clmr_ctrl->fw_boot_sem);
    if (tmp == 1) {
        complete(&clmr_ctrl->fw_boot_comp);
    } else {
        SHDISP_ERR("Unexpect isr error. Don't boot complete.[%d]\n", __LINE__);
 #ifdef SHDISP_RESET_LOG
        err_code.mode = SHDISP_DBG_MODE_LINUX;
        err_code.type = SHDISP_DBG_TYPE_CLMR_FW;
        err_code.code = SHDISP_DBG_CODE_UNEXPECT_HINT;
        err_code.subcode = SHDISP_DBG_SUBCODE_BOOT;
        shdisp_dbg_api_err_output(&err_code, 0);
 #endif /* SHDISP_RESET_LOG */
        shdisp_clmr_reg_dump_logset();
    }
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_wait4fw_cmd_comp                                     */
/*---------------------------------------------------------------------------*/
int shdisp_clmr_api_wait4fw_cmd_comp(unsigned char cmdno)
{
    unsigned long rc;
    struct shdisp_clmr_ctrl_t *clmr_ctrl;
    unsigned long waittime;
#ifdef SHDISP_RESET_LOG
    struct shdisp_dbg_error_code err_code;
#endif /* SHDISP_RESET_LOG */
    int ret;

    clmr_ctrl = &shdisp_clmr_ctrl;

    SHDISP_TRACE("in\n");
    SHDISP_PERFORMANCE_DEBUG("COMMON WAIT-FOR-FW-RESPONSE 0010 START\n");

    waittime = SHDISP_CLMR_WAIT4SYNC_TIMEOUT;
    rc = wait_for_completion_timeout(&clmr_ctrl->fw_cmd_comp, waittime);
    if (0 == rc) {
        ret = shdisp_clmr_hint_failsafe(&clmr_ctrl->fw_cmd_comp);
        if (ret == SHDISP_RESULT_SUCCESS) {
            return SHDISP_RESULT_SUCCESS;
        }
        SHDISP_ERR("timeout:[%d] cmdno=0x%x\n", __LINE__, cmdno);
#ifdef SHDISP_RESET_LOG
        err_code.mode = SHDISP_DBG_MODE_LINUX;
        err_code.type = SHDISP_DBG_TYPE_CLMR_FW;
        err_code.code = SHDISP_DBG_CODE_TIMEOUT;
        err_code.subcode = SHDISP_DBG_SUBCODE_COMMAND;
        shdisp_dbg_api_err_output(&err_code, 0);
#endif /* SHDISP_RESET_LOG */
        shdisp_clmr_reg_dump_logset();
        return SHDISP_RESULT_FAILURE;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_hint_failsafe                                            */
/*---------------------------------------------------------------------------*/
static int shdisp_clmr_hint_failsafe(struct completion *comp)
{
    struct shdisp_clmr_ctrl_t *clmr_ctrl = &shdisp_clmr_ctrl;
    int val;

    SHDISP_TRACE("in\n");

    val = gpio_get_value(SHDISP_GPIO_NUM_CLMR_HINT);
    if (val) {
        shdisp_workqueue_handler_clmr(&clmr_ctrl->work);
        if (comp->done) {
            comp->done--;
        }
        SHDISP_DEBUG("out1 HINT failsafe done.\n");
        return SHDISP_RESULT_SUCCESS;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_FAILURE;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_donefw_cmd_comp                                          */
/*---------------------------------------------------------------------------*/
static void shdisp_clmr_donefw_cmd_comp(void)
{
    struct shdisp_clmr_ctrl_t *clmr_ctrl;
    int tmp = 0;
#ifdef SHDISP_RESET_LOG
    struct shdisp_dbg_error_code err_code;
#endif /* SHDISP_RESET_LOG */

    clmr_ctrl = &shdisp_clmr_ctrl;

    SHDISP_TRACE("in\n");
    down(&clmr_ctrl->fw_cmd_sem);
    if (clmr_ctrl->fw_cmd_excute == 1) {
        tmp = 1;
    }
    clmr_ctrl->fw_cmd_excute = 0;
    up(&clmr_ctrl->fw_cmd_sem);
    if (tmp == 1) {
        complete(&clmr_ctrl->fw_cmd_comp);
    } else {
        SHDISP_ERR("Unexpect isr error. Don't cmd complete.[%d]\n", __LINE__);
 #ifdef SHDISP_RESET_LOG
        err_code.mode = SHDISP_DBG_MODE_LINUX;
        err_code.type = SHDISP_DBG_TYPE_CLMR_FW;
        err_code.code = SHDISP_DBG_CODE_UNEXPECT_HINT;
        err_code.subcode = SHDISP_DBG_SUBCODE_COMMAND;
        shdisp_dbg_api_err_output(&err_code, 0);
 #endif /* SHDISP_RESET_LOG */
        shdisp_clmr_reg_dump_logset();
    }
}


/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_wait4eDramPtr_rst_comp                               */
/*---------------------------------------------------------------------------*/
int shdisp_clmr_api_wait4eDramPtr_rst_comp(void)
{
    unsigned long rc;
    struct shdisp_clmr_ctrl_t *clmr_ctrl;
    int ret;
#ifdef SHDISP_RESET_LOG
    struct shdisp_dbg_error_code err_code;
#endif /* SHDISP_RESET_LOG */

    clmr_ctrl = &shdisp_clmr_ctrl;

    SHDISP_TRACE("in\n");

    if (shdisp_SYS_FWCMD_istimeoutexception()) {
        SHDISP_DEBUG(": FW timeout...\n");
        return SHDISP_RESULT_FAILURE;
    }

    rc = wait_for_completion_timeout(&clmr_ctrl->eDramPtr_rst_comp,
                                    SHDISP_CLMR_WAIT4SYNC_TIMEOUT);
    if (0 == rc) {
        ret = shdisp_clmr_hint_failsafe(&clmr_ctrl->eDramPtr_rst_comp);
        if (ret == SHDISP_RESULT_SUCCESS) {
            return SHDISP_RESULT_SUCCESS;
        }
        SHDISP_ERR("timeout:[%d] rc=%ld\n", __LINE__, rc);
#ifdef SHDISP_RESET_LOG
        err_code.mode = SHDISP_DBG_MODE_LINUX;
        err_code.type = SHDISP_DBG_TYPE_CLMR_FW;
        err_code.code = SHDISP_DBG_CODE_TIMEOUT;
        err_code.subcode = SHDISP_DBG_SUBCODE_MIF;
        shdisp_dbg_api_err_output(&err_code, 0);
#endif /* SHDISP_RESET_LOG */
        shdisp_clmr_reg_dump_logset();
        return SHDISP_RESULT_FAILURE;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_doneeDramPtr_rst_comp                                    */
/*---------------------------------------------------------------------------*/
static void shdisp_clmr_doneeDramPtr_rst_comp(void)
{
    struct shdisp_clmr_ctrl_t *clmr_ctrl;
    int tmp = 0;
#ifdef SHDISP_RESET_LOG
    struct shdisp_dbg_error_code err_code;
#endif /* SHDISP_RESET_LOG */

    clmr_ctrl = &shdisp_clmr_ctrl;

    SHDISP_TRACE("in\n");
    down(&clmr_ctrl->eDramPtr_rst_sem);
    if (clmr_ctrl->eDramPtr_rst_excute == 1) {
        tmp = 1;
    }
    clmr_ctrl->eDramPtr_rst_excute = 0;
    up(&clmr_ctrl->eDramPtr_rst_sem);
    if (tmp == 1) {
        complete(&clmr_ctrl->eDramPtr_rst_comp);
    } else {
        SHDISP_ERR("Unexpect isr error. Don't eDramPtr complete.[%d]\n", __LINE__);
 #ifdef SHDISP_RESET_LOG
        err_code.mode = SHDISP_DBG_MODE_LINUX;
        err_code.type = SHDISP_DBG_TYPE_CLMR_FW;
        err_code.code = SHDISP_DBG_CODE_UNEXPECT_HINT;
        err_code.subcode = SHDISP_DBG_SUBCODE_MIF;
        shdisp_dbg_api_err_output(&err_code, 0);
 #endif /* SHDISP_RESET_LOG */
        shdisp_clmr_reg_dump_logset();
    }
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_prepare_handshake                                    */
/*---------------------------------------------------------------------------*/
void shdisp_clmr_api_prepare_handshake(unsigned char cmdno, unsigned char *rbuf, int size)
{
    struct shdisp_clmr_handshake_t *clmr_handshake;

    clmr_handshake = &shdisp_clmr_handshake;

    clmr_handshake->cmdno = cmdno;
    clmr_handshake->rbuf  = rbuf;
    clmr_handshake->size  = size;
    clmr_handshake->err   = SHDISP_RESULT_FW_SUCCESS;

    shdisp_clmr_cmd_start();
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_get_handhake_error                                   */
/*---------------------------------------------------------------------------*/
int shdisp_clmr_api_get_handhake_error(void)
{
    int rtn = SHDISP_RESULT_SUCCESS;

    struct shdisp_clmr_handshake_t *clmr_handshake = &shdisp_clmr_handshake;
    rtn = clmr_handshake->err;
    if (rtn == SHDISP_RESULT_FW_ERR) {
        rtn = SHDISP_RESULT_FAILURE;
    } else if (rtn == SHDISP_RESULT_FW_ERR_DUMP) {
        shdisp_clmr_reg_dump_logset();
        rtn = SHDISP_RESULT_FAILURE;
    }
    clmr_handshake->err = SHDISP_RESULT_FW_SUCCESS;
    return rtn;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_request_irq                                              */
/*---------------------------------------------------------------------------*/
static int shdisp_clmr_request_irq(void)
{
    int rc;

    SHDISP_TRACE("in\n");

    rc = devm_request_irq(&shdisp_clmr_ctrl.pdev->dev, shdisp_clmr_irq, shdisp_clmr_int_isr,
            IRQF_TRIGGER_RISING,    "shdisp_clmr", &shdisp_clmr_ctrl);
    if (rc) {
        SHDISP_ERR("request_irq() failed. irq = 0x%x\n", shdisp_clmr_irq);
    }
    disable_irq(shdisp_clmr_irq);

    SHDISP_TRACE("out rc = %d\n", rc);

    return rc;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_enable_irq                                               */
/*---------------------------------------------------------------------------*/
static void shdisp_clmr_enable_irq(void)
{
    SHDISP_TRACE("in\n");

    enable_irq_wake(shdisp_clmr_irq);

    enable_irq(shdisp_clmr_irq);
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_disable_irq                                              */
/*---------------------------------------------------------------------------*/
static void shdisp_clmr_disable_irq(void)
{
    SHDISP_TRACE("in\n");

    disable_irq_wake(shdisp_clmr_irq);

    disable_irq(shdisp_clmr_irq);
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_tearint_log                                              */
/*---------------------------------------------------------------------------*/
static void shdisp_clmr_tearint_log(void)
{
    volatile const unsigned long curtime = jiffies;
    shdisp_clmr_ctrl.tear_int_count++;

    if ((time_after(curtime, shdisp_clmr_ctrl.tear_log_disable_time))
    ||  ((shdisp_clmr_ctrl.tear_log_disable_time == 0) && (shdisp_clmr_ctrl.tear_int_count == 1))) {
        SHDISP_WARN( "intterupt(tear block time) count=%u\n", shdisp_clmr_ctrl.tear_int_count );
        shdisp_clmr_ctrl.tear_log_disable_time = curtime + (HZ * 10);
    }
}

/*---------------------------------------------------------------------------*/
/*      shdisp_workqueue_handler_clmr_intr_donefw_cmd_comp                   */
/*---------------------------------------------------------------------------*/
#if defined(SHDISP_DEBUG_PROCFS_FW_ERROR)
static int shdisp_workqueue_handler_clmr_intr_donefw_cmd_comp(int check_fw_break, int check_fw_break_type)
#else  /* SHDISP_DEBUG_PROCFS_FW_ERROR */
static int shdisp_workqueue_handler_clmr_intr_donefw_cmd_comp(void)
#endif /* SHDISP_DEBUG_PROCFS_FW_ERROR */
{
    int ret = 0;
    struct shdisp_clmr_handshake_t *clmr_handshake;
#ifdef SHDISP_RESET_LOG
    struct shdisp_dbg_error_code err_code;
#endif /* SHDISP_RESET_LOG */

    clmr_handshake = &shdisp_clmr_handshake;

#if defined(SHDISP_DEBUG_PROCFS_FW_ERROR)
    if ((check_fw_break == SHDISP_CLMR_DBG_FW_ERROR) && (check_fw_break_type == SHDISP_CLMR_DBG_FW_HANDSHAKE)) {
        ret = -1;
    } else {
        ret = shdisp_SYS_variable_length_data_read(clmr_handshake->cmdno, clmr_handshake->rbuf, clmr_handshake->size);
        if ((check_fw_break == SHDISP_CLMR_DBG_FW_ERROR) && (check_fw_break_type == SHDISP_CLMR_DBG_FW_CMDNO_DIFFER)) {
            clmr_handshake->cmdno = 0;
        }
    }
#else /* SHDISP_DEBUG_PROCFS_FW_ERROR */
    ret = shdisp_SYS_variable_length_data_read(clmr_handshake->cmdno, clmr_handshake->rbuf, clmr_handshake->size);
#endif /* SHDISP_DEBUG_PROCFS_FW_ERROR */

    if (ret == -1) {
        SHDISP_ERR("HandShake Error!! clmr_handshake->cmdno=0x%02x\n", clmr_handshake->cmdno);
#ifdef SHDISP_RESET_LOG
        err_code.code = SHDISP_DBG_CODE_HANDSHAKE_ERROR;
        err_code.subcode = SHDISP_DBG_SUBCODE_NOT_COMPLETE;
#endif /* SHDISP_RESET_LOG */
        clmr_handshake->err = SHDISP_RESULT_FW_ERR_DUMP;
        goto fw_error;
    } else {
#if defined(SHDISP_DEBUG_PROCFS_FW_ERROR)
        if ((check_fw_break == SHDISP_CLMR_DBG_FW_ERROR) && (check_fw_break_type == SHDISP_CLMR_DBG_FW_ERROR_BIT_ON)) {
            ret |= 0x00000080;
        }
#endif /* SHDISP_DEBUG_PROCFS_FW_ERROR */
        if (clmr_handshake->cmdno == (ret & 0x0000007F)) {
            if ((ret & 0x00000080) != 0) {
                SHDISP_ERR("HandShake & CmdNo Success. But FW error bit ON. cmdno=0x%x\n", ret);
#ifdef SHDISP_RESET_LOG
                err_code.code = SHDISP_DBG_CODE_COMMAND_ERROR;
                err_code.subcode = SHDISP_DBG_SUBCODE_NONE;
#endif /* SHDISP_RESET_LOG */
                clmr_handshake->err = SHDISP_RESULT_FW_ERR;
                goto fw_error;
            }
            SHDISP_DEBUG("HandShake & CmdNo Success cmdno=0x%x\n", ret);
            clmr_handshake->err = SHDISP_RESULT_FW_SUCCESS;
        } else if ((ret & 0x00000080) != 0) {
            SHDISP_ERR("HandShake OK. But FW error bit ON & CmdNo differ. cmdno=0x%x FWcmdno=0x%x\n",
                                                                                    clmr_handshake->cmdno, ret);
#ifdef SHDISP_RESET_LOG
            err_code.code = SHDISP_DBG_CODE_COMMAND_ERROR;
            err_code.subcode = SHDISP_DBG_SUBCODE_NONE;
#endif /* SHDISP_RESET_LOG */
            clmr_handshake->err = SHDISP_RESULT_FW_ERR;
            goto fw_error;
        } else {
            SHDISP_ERR("HandShake OK. But CmdNo differ. cmdno=0x%x FWcmdno=0x%x\n", clmr_handshake->cmdno, ret);
#ifdef SHDISP_RESET_LOG
            err_code.code = SHDISP_DBG_CODE_HANDSHAKE_ERROR;
            err_code.subcode = SHDISP_DBG_SUBCODE_NUMBER_MISMATCH;
#endif /* SHDISP_RESET_LOG */
            clmr_handshake->err = SHDISP_RESULT_FW_ERR_DUMP;
            goto fw_error;
        }
    }
    clmr_handshake->cmdno = 0;
#if defined(SHDISP_DEBUG_PROCFS_FW_ERROR)
    if ((check_fw_break == SHDISP_CLMR_DBG_FW_OK) || (check_fw_break_type != SHDISP_CLMR_DBG_FW_TIMEOUT)) {
        shdisp_clmr_donefw_cmd_comp();
    }
#else
    shdisp_clmr_donefw_cmd_comp();
#endif /* SHDISP_DEBUG_PROCFS_FW_ERROR */
    return SHDISP_RESULT_SUCCESS;

fw_error:
#ifdef SHDISP_RESET_LOG
    err_code.mode = SHDISP_DBG_MODE_LINUX;
    err_code.type = SHDISP_DBG_TYPE_CLMR_FW;
    shdisp_dbg_api_err_output(&err_code, 0);
#endif /* SHDISP_RESET_LOG */
    clmr_handshake->cmdno = 0;
    shdisp_clmr_donefw_cmd_comp();
    return SHDISP_RESULT_FAILURE;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_workqueue_handler_clmr_intr_ARM_FW_start_comp                 */
/*---------------------------------------------------------------------------*/
static void shdisp_workqueue_handler_clmr_intr_ARM_FW_start_comp(void)
{
#if defined(SHDISP_DEBUG_PROCFS_FW_ERROR)
    if (!shdisp_debug_clmr_is_fw_break(0x02)) {
        shdisp_clmr_donefw_boot_comp();
    }
#else
    shdisp_clmr_donefw_boot_comp();
#endif /* SHDISP_DEBUG_PROCFS_FW_ERROR */
}

/*---------------------------------------------------------------------------*/
/*      shdisp_workqueue_handler_clmr_intr_doneeDramPtr_rst_comp             */
/*---------------------------------------------------------------------------*/
static void shdisp_workqueue_handler_clmr_intr_doneeDramPtr_rst_comp(void)
{
#if defined(SHDISP_DEBUG_PROCFS_FW_ERROR)
    if (!shdisp_debug_clmr_is_fw_break(0x04)) {
        shdisp_clmr_doneeDramPtr_rst_comp();
    }
#else
    shdisp_clmr_doneeDramPtr_rst_comp();
#endif /* SHDISP_DEBUG_PROCFS_FW_ERROR */
}

/*---------------------------------------------------------------------------*/
/*      shdisp_workqueue_handler_clmr_intr_error_other                       */
/*---------------------------------------------------------------------------*/
static void shdisp_workqueue_handler_clmr_intr_error_other(void)
{
    int ret;
    int cmdno = 0xFFFFFFFF;
    unsigned char tempHeap[32];
#ifdef SHDISP_RESET_LOG
    struct shdisp_dbg_error_code err_code;
#endif /* SHDISP_RESET_LOG */

    memset(tempHeap, 0, 32);
    ret = shdisp_SYS_variable_length_data_read(cmdno, tempHeap, 32);
    SHDISP_ERR("INTSET1 bit3 handshake rtn=%d\n", ret);
#ifdef SHDISP_RESET_LOG
    err_code.mode = SHDISP_DBG_MODE_LINUX;
    err_code.type = SHDISP_DBG_TYPE_CLMR_FW;
    err_code.code = SHDISP_DBG_CODE_ASYNC_ERROR;
    err_code.subcode = SHDISP_DBG_SUBCODE_NONE;
    shdisp_dbg_api_err_output(&err_code, 0);
#endif /* SHDISP_RESET_LOG */
    shdisp_clmr_reg_dump_logset();
}

/*---------------------------------------------------------------------------*/
/*      shdisp_workqueue_handler_clmr_intr_lcd_det                           */
/*---------------------------------------------------------------------------*/
static void shdisp_workqueue_handler_clmr_intr_lcd_det(void)
{
#ifdef SHDISP_RESET_LOG
    struct shdisp_dbg_error_code err_code;
    unsigned char subcode;
#endif /* SHDISP_RESET_LOG */

#ifdef SHDISP_RESET_LOG
    subcode = shdisp_dbg_get_subcode();
    if ((subcode != SHDISP_DBG_SUBCODE_DET_LOW) && (subcode != SHDISP_DBG_SUBCODE_DISPON_NG)) {
        err_code.mode = SHDISP_DBG_MODE_LINUX;
        err_code.type = SHDISP_DBG_TYPE_PANEL;
        err_code.code = SHDISP_DBG_CODE_ERROR_DETECT;
        err_code.subcode = SHDISP_DBG_SUBCODE_ESD_MIPI;
        shdisp_dbg_api_err_output(&err_code, 0);
        shdisp_dbg_set_subcode(SHDISP_DBG_SUBCODE_ESD_MIPI);
    }
#endif /* SHDISP_RESET_LOG */
    if (shdisp_api_do_lcdc_mipi_dsi_det_recovery() != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("recovery request error!!\n");
    }
}

/*---------------------------------------------------------------------------*/
/*      shdisp_workqueue_handler_clmr_intr_psals_det                         */
/*---------------------------------------------------------------------------*/
static void shdisp_workqueue_handler_clmr_intr_psals_det(void)
{
#ifdef SHDISP_RESET_LOG
    struct shdisp_dbg_error_code err_code;

    err_code.mode = SHDISP_DBG_MODE_LINUX;
    err_code.type = SHDISP_DBG_TYPE_PSALS;
    err_code.code = SHDISP_DBG_CODE_ERROR_DETECT;
    err_code.subcode = SHDISP_DBG_SUBCODE_I2C_ERROR_CLMR;
    shdisp_dbg_api_err_output(&err_code, 0);
#endif /* SHDISP_RESET_LOG */
#ifndef SHDISP_NOT_SUPPORT_DET
    if (shdisp_api_do_psals_recovery() != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("recovery request error!!\n");
    }
#endif
    return;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_workqueue_handler_clmr                                        */
/*---------------------------------------------------------------------------*/
static void shdisp_workqueue_handler_clmr(struct work_struct *work)
{
    unsigned char regINTR[4] = {0};
    unsigned char reg[8] = {0};
    unsigned char clr[4] = {0, 0, 0, 0};
#if defined(SHDISP_DEBUG_PROCFS_FW_ERROR)
    int check_fw_break = SHDISP_CLMR_DBG_FW_OK;
    int check_fw_break_type = SHDISP_CLMR_DBG_FW_NONE;
#endif /* SHDISP_DEBUG_PROCFS_FW_ERROR */
    struct shdisp_clmr_ctrl_t *clmr_ctrl = &shdisp_clmr_ctrl;
#if defined(CONFIG_ANDROID_ENGINEERING)
#ifdef SHDISP_RESET_LOG
    struct shdisp_dbg_error_code err_code;
#endif /* SHDISP_RESET_LOG */
#endif /* defined(CONFIG_ANDROID_ENGINEERING) */

    down(&clmr_ctrl->wq_sem);

    SHDISP_TRACE("in\n");

    shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_REG_INTR, NULL, 0, regINTR, 4);

    if (!(*(unsigned int *)regINTR)) {
        SHDISP_WARN("HINT interruption occurred. But not set Clmr INTR register.\n");
#if defined(CONFIG_ANDROID_ENGINEERING)
#ifdef SHDISP_RESET_LOG
        err_code.mode = SHDISP_DBG_MODE_LINUX;
        err_code.type = SHDISP_DBG_TYPE_CLMR_HW;
        err_code.code = SHDISP_DBG_CODE_UNKNOWN_HINT;
        err_code.subcode = SHDISP_DBG_SUBCODE_NONE;
        shdisp_dbg_api_err_output(&err_code, 0);
#endif /* SHDISP_RESET_LOG */
#endif /* defined(CONFIG_ANDROID_ENGINEERING) */
    }

    while (regINTR[1] & 0x03) {

        if (regINTR[1] & 0x01) {
            shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_REG_INTSET1, NULL, 0, &reg[0], 4);
            shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_REG_INTSET1, clr, sizeof(clr), NULL, 0);

            if ((reg[3] & 0x01) == 0x01) {
#if defined(SHDISP_DEBUG_PROCFS_FW_ERROR)
                if (shdisp_debug_clmr_is_fw_break(0x01)) {
                    check_fw_break = SHDISP_CLMR_DBG_FW_ERROR;
                    check_fw_break_type = shdisp_debug_clmr_fw_error_pattern();
                    if (check_fw_break_type == SHDISP_CLMR_DBG_FW_INTSET1) {
                        reg[3] = 0x08;
                    }
                }
                shdisp_workqueue_handler_clmr_intr_donefw_cmd_comp(check_fw_break, check_fw_break_type);
#else  /* SHDISP_DEBUG_PROCFS_FW_ERROR */
                shdisp_workqueue_handler_clmr_intr_donefw_cmd_comp();
#endif /* SHDISP_DEBUG_PROCFS_FW_ERROR */
            }
            if ((reg[3] & 0x02) == 0x02) {
                SHDISP_DEBUG("Startup_complete_ARM_FW !!\n");
                shdisp_workqueue_handler_clmr_intr_ARM_FW_start_comp();
            }
            if ((reg[3] & 0x04) == 0x04) {
                SHDISP_DEBUG("eDramPtr Reset complete\n");
                shdisp_workqueue_handler_clmr_intr_doneeDramPtr_rst_comp();
            }
            if ((reg[3] & 0x08) == 0x08) {
                shdisp_workqueue_handler_clmr_intr_error_other();
            }
        }

        if (regINTR[1] & 0x02) {
            shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_REG_INTSET2, NULL, 0, &reg[4], 4);
            shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_REG_INTSET2, clr, sizeof(clr), NULL, 0);

            if (reg[7] & 0x01) {
                SHDISP_ERR("lcd det INTSET2=0x%02x\n", reg[7]);
                shdisp_workqueue_handler_clmr_intr_lcd_det();
            }

            if (reg[7] & 0x02) {
                SHDISP_ERR("psals det INTSET2=0x%02x\n", reg[7]);
                shdisp_workqueue_handler_clmr_intr_psals_det();
            }

            if (reg[7] & 0x04) {
                shdisp_clmr_tearint_log();
            }
        }
        (*(unsigned int *)regINTR) = 0;
        shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_REG_INTR, NULL, 0, regINTR, 4);
        if ((regINTR[1] & 0x03) != 0x00) {
            SHDISP_DEBUG("HINT interruption occurred in workqueue process. INTR[1]=0x%02X\n", regINTR[1]);
        }
    }
    SHDISP_TRACE("out\n");
    up(&clmr_ctrl->wq_sem);
    wake_unlock(&clmr_ctrl->wake_lock);
}

/*---------------------------------------------------------------------------*/
/*      interrupt context                                                    */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_int_isr                                                  */
/*---------------------------------------------------------------------------*/
static irqreturn_t shdisp_clmr_int_isr(int irq_num, void *data)
{
    unsigned long flags;
    int ret;
    struct shdisp_clmr_ctrl_t *clmr_ctrl;

    SHDISP_TRACE("in\n");
    SHDISP_PERFORMANCE_DEBUG("COMMON WAIT-FOR-FW-RESPONSE 0010 END\n");

    clmr_ctrl = &shdisp_clmr_ctrl;

    spin_lock_irqsave(&clmr_ctrl->spin_lock, flags);
    if (clmr_ctrl->workqueue) {
        wake_lock(&clmr_ctrl->wake_lock);
        ret = queue_work(clmr_ctrl->workqueue, &clmr_ctrl->work);
        if (ret == 0) {
            wake_unlock(&clmr_ctrl->wake_lock);
            SHDISP_DEBUG("queue_work failed.\n");
        }
    }
    spin_unlock_irqrestore(&clmr_ctrl->spin_lock, flags);

    SHDISP_TRACE("out\n");

    return IRQ_HANDLED;
}

/*---------------------------------------------------------------------------*/
/*      tools                                                                */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_or_readModifyWrite                                       */
/*---------------------------------------------------------------------------*/
static int shdisp_clmr_or_readModifyWrite(
                        unsigned short reg, unsigned long lData)
{
    int rc;
    int bufSize = sizeof(char) * 4;
    unsigned char cBuf[4];

    rc = shdisp_SYS_clmr_sio_transfer(reg, NULL, 0, &cBuf[0], bufSize);
    if (SHDISP_RESULT_SUCCESS == rc) {
        cBuf[0] |= (unsigned char)((lData >> 24) & 0x000000FF);
        cBuf[1] |= (unsigned char)((lData >> 16) & 0x000000FF);
        cBuf[2] |= (unsigned char)((lData >>  8) & 0x000000FF);
        cBuf[3] |= (unsigned char)( lData        & 0x000000FF);

        rc = shdisp_SYS_clmr_sio_transfer(reg, &cBuf[0], bufSize, NULL, 0);
    }

    return rc;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_and_readModifyWrite                                      */
/*---------------------------------------------------------------------------*/
static int shdisp_clmr_and_readModifyWrite(
                        unsigned short reg, unsigned long lMask)
{
    int rc;
    int bufSize = sizeof(char) * 4;
    unsigned char cBuf[4];

    rc = shdisp_SYS_clmr_sio_transfer(reg, NULL, 0, &cBuf[0], bufSize);
    if (SHDISP_RESULT_SUCCESS == rc) {
        cBuf[0] &= (unsigned char)((lMask >> 24) & 0x000000FF);
        cBuf[1] &= (unsigned char)((lMask >> 16) & 0x000000FF);
        cBuf[2] &= (unsigned char)((lMask >>  8) & 0x000000FF);
        cBuf[3] &= (unsigned char)( lMask        & 0x000000FF);

        rc = shdisp_SYS_clmr_sio_transfer(reg, &cBuf[0], bufSize, NULL, 0);
    }

    return rc;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_readModifyWrite                                          */
/*---------------------------------------------------------------------------*/
static int shdisp_clmr_readModifyWrite(unsigned short reg,
                        unsigned long lData, unsigned long lMask)
{
    int rc;
    int bufSize = sizeof(char) * 4;
    unsigned char cData[4];
    unsigned char cBuf[4];

    rc = shdisp_SYS_clmr_sio_transfer(reg, NULL, 0, &cBuf[0], bufSize);
    if (SHDISP_RESULT_SUCCESS == rc) {
        cData[0] = cBuf[0] & (unsigned char)((lMask >> 24) & 0x000000FF);
        cData[1] = cBuf[1] & (unsigned char)((lMask >> 16) & 0x000000FF);
        cData[2] = cBuf[2] & (unsigned char)((lMask >>  8) & 0x000000FF);
        cData[3] = cBuf[3] & (unsigned char)( lMask        & 0x000000FF);

        cData[0] |= (unsigned char)((lData >> 24) & 0x000000FF);
        cData[1] |= (unsigned char)((lData >> 16) & 0x000000FF);
        cData[2] |= (unsigned char)((lData >>  8) & 0x000000FF);
        cData[3] |= (unsigned char)( lData        & 0x000000FF);

        rc = shdisp_SYS_clmr_sio_transfer(reg, &cData[0], bufSize, NULL, 0);
    }

    return rc;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_regSet                                                   */
/*---------------------------------------------------------------------------*/
static int shdisp_clmr_regSet(const shdisp_clmrRegSetting_t *dat)
{
    int rc = SHDISP_RESULT_SUCCESS;
    int bufSize = sizeof(char) * 4;
    int wait = dat->wait;
    union uData_t {
        unsigned long lBuf;
        unsigned char cBuf[4];
    } uData;

    switch (dat->flg) {
    case CALI_AND:
        rc = shdisp_clmr_and_readModifyWrite(dat->addr, dat->mask);
        break;
    case CALI_RMW:
        rc = shdisp_clmr_readModifyWrite(dat->addr, dat->data, dat->mask);
        break;
    case CALI_STR:
        uData.lBuf = htonl(dat->data);
        rc = shdisp_SYS_clmr_sio_transfer(dat->addr, &uData.cBuf[0], bufSize, NULL, 0);
        break;
    case CALI_OR:
    default:
        rc = shdisp_clmr_or_readModifyWrite(dat->addr, dat->data);
        break;
    }

    if (0 != wait) {
        shdisp_SYS_delay_us(wait);
    }

    return rc;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_regSet_multi                                             */
/*---------------------------------------------------------------------------*/
static int shdisp_clmr_regSet_multi(const shdisp_clmrRegSetting_t *regtable, int size)
{
    int rc = SHDISP_RESULT_SUCCESS;

    unsigned char setdata[64] = {0};

    unsigned char setdatasize = 0;

    int i;
    shdisp_clmrRegSetting_t *dat;
    dat = (shdisp_clmrRegSetting_t *)regtable;

    for (i = 0; i < size; i++) {

        switch (dat->flg) {
        case CALI_STRM:
            if (setdatasize + SHDISP_CLMR_FWCMD_HOST_WORD_WRITE_SIZE > 64) {
                shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_MULTI_WRITE, setdatasize, setdata);
                setdatasize = 0;
            }

            setdata[setdatasize] =  dat->addr & 0x00FF;
            setdata[setdatasize + 1] = (dat->addr & 0xFF00) >> 8;
            setdata[setdatasize + 2] =  dat->data & 0x000000FF;
            setdata[setdatasize + 3] = (dat->data & 0x0000FF00) >> 8;
            setdata[setdatasize + 4] = (dat->data & 0x00FF0000) >> 16;
            setdata[setdatasize + 5] = (dat->data & 0xFF000000) >> 24;

            setdatasize += SHDISP_CLMR_FWCMD_HOST_WORD_WRITE_SIZE;

            if (dat->wait > 0) {
                shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_MULTI_WRITE, setdatasize, setdata);
                setdatasize = 0;
                shdisp_SYS_cmd_delay_us(dat->wait);
            }
            break;

        case CALI_OR:
        case CALI_AND:
        case CALI_RMW:
        case CALI_STR:
            if (setdatasize) {
                shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_MULTI_WRITE, setdatasize, setdata);
                setdatasize = 0;
            }

            rc = shdisp_clmr_regSetwithFW(dat);

            break;

        default:
            SHDISP_ERR("error. dat->flg = %d\n", dat->flg);
            return SHDISP_RESULT_FAILURE;
        }

        dat++;
    }

    if (setdatasize) {
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_MULTI_WRITE, setdatasize, setdata);
    }

    return rc;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_regSetwithFW                                             */
/*---------------------------------------------------------------------------*/
static int shdisp_clmr_regSetwithFW(const shdisp_clmrRegSetting_t *dat)
{
    int rc = SHDISP_RESULT_SUCCESS;
    int isdoKick;
    unsigned char bufary[10];
    unsigned char fwcmd;
    unsigned short len;

    isdoKick = !shdisp_FWCMD_buf_get_nokick();

    if (isdoKick) {
        shdisp_FWCMD_buf_init(0);
    }

    (*(unsigned short *)(bufary))   = dat->addr;

    switch (dat->flg) {
    case CALI_OR:
        fwcmd = SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE;
        len = 10;
        *((unsigned long *)(bufary + 2)) = dat->data;
        *((unsigned long *)(bufary + 6)) = dat->data;
        break;
    case CALI_AND:
        fwcmd = SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE;
        len = 10;
        *((unsigned long *)(bufary + 2)) = ~(dat->mask);
        *((unsigned long *)(bufary + 6)) = 0;
        break;
    case CALI_RMW:
        fwcmd = SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE;
        len = 10;
        *((unsigned long *)(bufary + 2)) = ~(dat->mask) | dat->data;
        *((unsigned long *)(bufary + 6)) = dat->data;
        break;
    case CALI_STR:
    default:
        fwcmd = SHDISP_CLMR_FWCMD_HOST_1WORD_WRITE;
        len = 6;
        *((unsigned long *)(bufary + 2)) = dat->data;
        break;
    }

    shdisp_FWCMD_buf_add(fwcmd, len, bufary);

    if (0 != dat->wait) {
        shdisp_SYS_cmd_delay_us(dat->wait);
    }

    if (isdoKick) {
        shdisp_FWCMD_buf_finish();
        rc = shdisp_FWCMD_doKick(1, 0, NULL);
        if (rc != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR(" dokick failed.\n");
        }
    }

    return rc;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_regsSetbyFW                                              */
/*---------------------------------------------------------------------------*/
static int shdisp_clmr_regsSetbyFW(const shdisp_clmrRegSetting_t *regtables, int size)
{
    int ret = SHDISP_RESULT_SUCCESS;
    int isdoKick;
    unsigned char wdata[SHDISP_CLMR_FWCMD_HOST_MULTI_WRITE_MAX_SIZE];
    unsigned short datalen = 0;
    const shdisp_clmrRegSetting_t *regsettings = regtables;
    int i;

    isdoKick = !shdisp_FWCMD_buf_get_nokick();

    if (isdoKick) {
        shdisp_FWCMD_buf_set_nokick(1);
        shdisp_FWCMD_buf_init(0);
    }

    for (i = 0; i < size; i++) {
        if (regsettings->flg == CALI_RMW) {
            if (datalen) {
                shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_MULTI_WRITE, datalen, wdata);
                datalen = 0;
            }

            *((unsigned short *)(&wdata[0])) =  regsettings->addr;
            *( (unsigned long *)(&wdata[2])) = ~(regsettings->mask);
            *( (unsigned long *)(&wdata[6])) = (regsettings->data & ~(regsettings->mask));
            shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE, SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE_SIZE,
                                                                                                                wdata);
        } else if (regsettings->flg == CALI_STR) {
            if (datalen + SHDISP_CLMR_FWCMD_HOST_MULTI_WRITE_ONE_SIZE > SHDISP_CLMR_FWCMD_HOST_MULTI_WRITE_MAX_SIZE) {
                shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_MULTI_WRITE, datalen, wdata);
                datalen = 0;
            }

            *((unsigned short *)(&wdata[datalen    ])) = regsettings->addr;
            *( (unsigned long *)(&wdata[datalen + 2])) = regsettings->data;
            datalen += SHDISP_CLMR_FWCMD_HOST_MULTI_WRITE_ONE_SIZE;
        } else {
            SHDISP_ERR("<INVALID_VALUE> flg = %d.\n", regsettings->flg);
            return SHDISP_RESULT_FAILURE;
        }

        if (0 != regsettings->wait) {
            if (datalen) {
                shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_MULTI_WRITE, datalen, wdata);
                datalen = 0;
            }
            shdisp_SYS_cmd_delay_us(regsettings->wait);
        }
        regsettings++;
    }

    if (datalen) {
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_MULTI_WRITE, datalen, wdata);
    }

    if (isdoKick) {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
        shdisp_FWCMD_buf_set_nokick(0);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("dokick failed.\n");
        }
    }

    return ret;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_set_dsctl                                                */
/*---------------------------------------------------------------------------*/
static int shdisp_clmr_set_dsctl(void)
{
    union uData_t {
        unsigned long lBuf;
        unsigned char cBuf[4];
    } uData;

    SHDISP_TRACE("in\n");

    uData.lBuf = htonl(SHDISP_CLMR_SET_DSCTL);
    shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_REG_DSCTL, uData.cBuf, 4, NULL, 0);
    return SHDISP_RESULT_SUCCESS;
}
/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_config_reg                                               */
/*---------------------------------------------------------------------------*/
static int shdisp_clmr_config_reg(clmr_vreg_t *vreg,
                                    int num,
                                    struct regulator **reg_ptr,
                                    int config)
{
    int rc = 0;
    int i = 0;
    clmr_vreg_t *curr_vreg;

    SHDISP_TRACE("in\n");

    if (1 == config) {
        for (i = 0; i < num; i++) {
            curr_vreg = &vreg[i];
            reg_ptr[i] = devm_regulator_get( &(shdisp_clmr_ctrl.pdev->dev), curr_vreg->reg_name );
            if (IS_ERR(reg_ptr[i])) {
                SHDISP_ERR("%s get failed.\n",
                    curr_vreg->reg_name);
                reg_ptr[i] = NULL;
                goto vreg_get_fail;
            }

            if (curr_vreg->type == REG_LDO) {
                rc = regulator_set_voltage(reg_ptr[i],
                                            curr_vreg->min_voltage,
                                            curr_vreg->max_voltage);
                if (rc < 0) {
                    SHDISP_ERR("%s set_voltage failed, rc=%d\n",
                                            curr_vreg->reg_name, rc);
                    goto vreg_set_voltage_fail;
                }

                if (curr_vreg->op_mode >= 0) {
                    rc = regulator_set_optimum_mode(reg_ptr[i],
                                                    curr_vreg->op_mode);
                    if (rc < 0) {
                        SHDISP_ERR("%s set_optimum_mode failed, rc=%d\n",
                                                curr_vreg->reg_name, rc);
                        goto vreg_set_opt_mode_fail;
                    }
                }
            }
        }
    } else {
        for (i = num - 1; i >= 0; i--) {
            curr_vreg = &vreg[i];
            if (reg_ptr[i] != NULL) {
                if (curr_vreg->type == REG_LDO) {
                    if (curr_vreg->op_mode >= 0) {
                        regulator_set_optimum_mode(
                            reg_ptr[i], 0);
                    }
                    regulator_set_voltage(
                        reg_ptr[i], 0, curr_vreg->
                        max_voltage);
                }
                regulator_put(reg_ptr[i]);
                reg_ptr[i] = NULL;
            }
        }
    }

    SHDISP_TRACE("out\n");

    return SHDISP_RESULT_SUCCESS;

vreg_unconfig:
    if (curr_vreg->type == REG_LDO) {
        regulator_set_optimum_mode(reg_ptr[i], 0);
    }

vreg_set_opt_mode_fail:
    if (curr_vreg->type == REG_LDO) {
        regulator_set_voltage(reg_ptr[i], 0,
            curr_vreg->max_voltage);
    }

vreg_set_voltage_fail:
    regulator_put(reg_ptr[i]);
    reg_ptr[i] = NULL;

vreg_get_fail:
    for (i--; i >= 0; i--) {
        curr_vreg = &vreg[i];
        goto vreg_unconfig;
    }

    SHDISP_DEBUG("error.\n");

    return -ENODEV;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_enable_reg                                               */
/*---------------------------------------------------------------------------*/
static int shdisp_clmr_enable_reg(clmr_vreg_t *vreg,
                                    int num,
                                    struct regulator **reg_ptr,
                                    int enable)
{
    int rc = 0;
    int i = 0;

    SHDISP_TRACE("in\n");

    if (vreg == NULL) {
        return SHDISP_RESULT_FAILURE;
    }
    if (reg_ptr == NULL) {
        return SHDISP_RESULT_FAILURE;
    }
    if (SHDISP_CLMR_ENABLE == enable) {
        rc = gpio_request(shdisp_clmr_ctrl.core_reg_gpio, "CLMR_REG_CORE");
        if (rc < 0) {
            SHDISP_ERR("gpio_request error gpio=%d, rc=%d\n", shdisp_clmr_ctrl.core_reg_gpio, rc);
            goto gpio_req_err_out;
        }
        for (i = 0; i < num; i++) {
            if (IS_ERR(reg_ptr[i])) {
                SHDISP_ERR("%s null regulator\n",
                    vreg[i].reg_name);
                rc = SHDISP_RESULT_FAILURE;
                goto disable_vreg;
            }

            rc = regulator_enable(reg_ptr[i]);
            if (rc < 0) {
                SHDISP_ERR("%s enable failed, rc=%d\n",
                                    vreg[i].reg_name, rc);
                goto disable_vreg;
            }
            if (vreg[i].delay > 0) {
                shdisp_SYS_delay_us(vreg[i].delay);
            }
        }
        gpio_set_value(shdisp_clmr_ctrl.core_reg_gpio, 1);
    } else {
        gpio_set_value(shdisp_clmr_ctrl.core_reg_gpio, 0);
        gpio_free(shdisp_clmr_ctrl.core_reg_gpio);
        for (i = num - 1; i >= 0; i--) {
            rc = regulator_disable(reg_ptr[i]);
            if (rc) {
                SHDISP_ERR("%s disable failed, rc=%d\n",
                                    vreg[i].reg_name, rc);
            }
        }
    }

    SHDISP_TRACE("out\n");

    return SHDISP_RESULT_SUCCESS;

disable_vreg:
    for (i--; i >= 0; i--) {
        regulator_disable(reg_ptr[i]);
        if (vreg[i].delay > 0) {
            shdisp_SYS_delay_us(vreg[i].delay);
        }
    }
    gpio_free(shdisp_clmr_ctrl.core_reg_gpio);
gpio_req_err_out:

    SHDISP_DEBUG("error. rc=%d\n", rc);

    return rc;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_ewb_cross_lut_tbl                                             */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_ewb_cross_lut_tbl(unsigned short mode, unsigned short ap_type)
{
    int i;
    int quotient;
    int remainder;
    int delta;
    const unsigned short *corss_lut_tbl;

    SHDISP_TRACE("in\n");

    if (mode == SHDISP_MAIN_DISP_PIC_ADJ_MODE_00) {
        memcpy(&clmr_ewb_accu_cross, clmr_ewb_accu, sizeof(clmr_ewb_accu_cross));
        SHDISP_TRACE("out\n");
        return SHDISP_RESULT_SUCCESS;
    }

    corss_lut_tbl = ewb_cross_lut_R[mode - 1][ap_type];
    for (i = 0; i < SHDISP_LCDC_EWB_TBL_SIZE; i++) {
        quotient  = corss_lut_tbl[i] / 4;
        remainder = corss_lut_tbl[i] % 4;
        if (quotient + 1 == SHDISP_LCDC_EWB_TBL_SIZE) {
            delta = 0;
        } else {
            delta = (clmr_ewb_accu[0].valR[quotient + 1] - clmr_ewb_accu[0].valR[quotient]) * 100 / 4;
        }
        clmr_ewb_accu_cross.valR[i] = clmr_ewb_accu[0].valR[quotient] + ((delta * remainder) / 100);
    }

    corss_lut_tbl = ewb_cross_lut_G[mode - 1][ap_type];
    for (i = 0; i < SHDISP_LCDC_EWB_TBL_SIZE; i++) {
        quotient  = corss_lut_tbl[i] / 4;
        remainder = corss_lut_tbl[i] % 4;
        if (quotient + 1 == SHDISP_LCDC_EWB_TBL_SIZE) {
            delta = 0;
        } else {
            delta = (clmr_ewb_accu[0].valG[quotient + 1] - clmr_ewb_accu[0].valG[quotient]) * 100 / 4;
        }
        clmr_ewb_accu_cross.valG[i] = clmr_ewb_accu[0].valG[quotient] + ((delta * remainder) / 100);
    }

    corss_lut_tbl = ewb_cross_lut_B[mode - 1][ap_type];
    for (i = 0; i < SHDISP_LCDC_EWB_TBL_SIZE; i++) {
        quotient  = corss_lut_tbl[i] / 4;
        remainder = corss_lut_tbl[i] % 4;
        if (quotient + 1 == SHDISP_LCDC_EWB_TBL_SIZE) {
            delta = 0;
        } else {
            delta = (clmr_ewb_accu[0].valB[quotient + 1] - clmr_ewb_accu[0].valB[quotient]) * 100 / 4;
        }
        clmr_ewb_accu_cross.valB[i] = clmr_ewb_accu[0].valB[quotient] + ((delta * remainder) / 100);
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_api_set_pic_adj_param                                         */
/* ------------------------------------------------------------------------- */
int shdisp_clmr_api_set_pic_adj_param(struct shdisp_main_pic_adj *pic_adj)
{
    int ret = SHDISP_RESULT_SUCCESS;
    const unsigned char *now;
    unsigned char set[NUM_PIC_ADJ_MATRIX];
    unsigned char off[NUM_PIC_ADJ_MATRIX];
    int i;
    unsigned char type = SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_OFF;

    SHDISP_TRACE("in\n");
    SHDISP_DEBUG("now mode = %d req mode = %d", clmr_pic_adj.mode, pic_adj->mode);

#ifdef KERNEL_CALL_PIC_ADJ_MDP
    ret = shdisp_clmr_set_pic_adj_data(pic_adj->mode, clmr_ap_type);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_set_pic_adj_data Error!!!\n");
        return ret;
    }
#endif

    if ((pic_adj->mode < SHDISP_MAIN_DISP_PIC_ADJ_MODE_00) || (pic_adj->mode >= NUM_SHDISP_MAIN_DISP_PIC_ADJ_MODE)) {
        SHDISP_ERR("<INVALID_VALUE> mode = %d.\n", pic_adj->mode);
        return SHDISP_RESULT_FAILURE;
    }
    if (clmr_pic_adj.mode == pic_adj->mode) {
        SHDISP_DEBUG("<INVALID_VALUE> Same Parameter.\n");
        return SHDISP_RESULT_SUCCESS;
    }

    if (shdisp_pm_is_clmr_on() != SHDISP_DEV_STATE_ON) {
        clmr_pic_adj.mode = pic_adj->mode;
        SHDISP_DEBUG("CLMR IS NOT ACTIVE.\n");
        return SHDISP_RESULT_SUCCESS;
    }

    if (clmr_trv_info.status == SHDISP_CLMR_TRV_ON) {
        clmr_pic_adj.mode = pic_adj->mode;
        SHDISP_DEBUG("TRV ON.\n");
        return SHDISP_RESULT_SUCCESS;
    }

    now = shdisp_clmr_get_pic_adj_matrix(clmr_trv_info.status, &clmr_dbc, &clmr_pic_adj, clmr_ap_type);
    SHDISP_DEBUG("now = SVCT(%d), HSV(%d), PCA(%d), CPF(%d), AE(%d), SBL(%d), SMITE(%d), TRV(%d)\n",
                     now[PIC_ADJ_MATRIX_SVCT],  now[PIC_ADJ_MATRIX_HSV], now[PIC_ADJ_MATRIX_PCA],
                     now[PIC_ADJ_MATRIX_CPF],   now[PIC_ADJ_MATRIX_AE],  now[PIC_ADJ_MATRIX_SBL],
                     now[PIC_ADJ_MATRIX_SMITE], now[PIC_ADJ_MATRIX_TRV]);
    memcpy(set, shdisp_clmr_get_pic_adj_matrix(clmr_trv_info.status, &clmr_dbc, pic_adj, clmr_ap_type),
                                                                                    NUM_PIC_ADJ_MATRIX);
    SHDISP_DEBUG("set = SVCT(%d), HSV(%d), PCA(%d), CPF(%d), AE(%d), SBL(%d), SMITE(%d), TRV(%d)\n",
                     set[PIC_ADJ_MATRIX_SVCT],  set[PIC_ADJ_MATRIX_HSV], set[PIC_ADJ_MATRIX_PCA],
                     set[PIC_ADJ_MATRIX_CPF],   set[PIC_ADJ_MATRIX_AE],  set[PIC_ADJ_MATRIX_SBL],
                     set[PIC_ADJ_MATRIX_SMITE], set[PIC_ADJ_MATRIX_TRV]);

    for (i = 0; i < NUM_PIC_ADJ_MATRIX; i++) {
        off[i] = ((now[i] != 0) && (set[i] == 0)) ? now[i] : 0;
    }
    SHDISP_DEBUG("off = SVCT(%d), HSV(%d), PCA(%d), CPF(%d), AE(%d), SBL(%d), SMITE(%d), TRV(%d)\n",
                     off[PIC_ADJ_MATRIX_SVCT],  off[PIC_ADJ_MATRIX_HSV], off[PIC_ADJ_MATRIX_PCA],
                     off[PIC_ADJ_MATRIX_CPF],   off[PIC_ADJ_MATRIX_AE],  off[PIC_ADJ_MATRIX_SBL],
                     off[PIC_ADJ_MATRIX_SMITE], off[PIC_ADJ_MATRIX_TRV]);

    ret = shdisp_clmr_sqe_pic_adj_off(off);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_sqe_pic_adj_off Error!!!\n");
        return ret;
    }

    if (now[PIC_ADJ_MATRIX_SBL] == SHDISP_CLMR_SBL_ACC)
        set[PIC_ADJ_MATRIX_SBL] = SHDISP_CLMR_SBL_ACC;

    ret = shdisp_clmr_sqe_pic_adj_on(pic_adj, clmr_ap_type, set);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_sqe_pic_adj_on Error!!!\n");
        return ret;
    }

    ret = shdisp_clmr_ewb_cross_lut_tbl(pic_adj->mode, clmr_ap_type);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_ewb_cross_lut_tbl Error!!!\n");
        return ret;
    }
    ret = shdisp_clmr_sqe_ewb_lut_chg();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_api_set_ewb_tbl Error!!!\n");
        return ret;
    }

    if ((set[PIC_ADJ_MATRIX_CPF] != off[PIC_ADJ_MATRIX_CPF]) ||
        (set[PIC_ADJ_MATRIX_SBL] != off[PIC_ADJ_MATRIX_SBL])) {

        if (set[PIC_ADJ_MATRIX_CPF] &&
            (pic_adj->mode != SHDISP_MAIN_DISP_PIC_ADJ_MODE_00) &&
            cpf_on_param[pic_adj->mode - 1][clmr_ap_type][0]) {
            type |= SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_CPF;
        }
        if ((set[PIC_ADJ_MATRIX_SBL] == SHDISP_CLMR_SBL_ACC)
         || ((set[PIC_ADJ_MATRIX_SBL] == SHDISP_CLMR_SBL_AE) &&
             (sbl_on_param[SHDISP_CLMR_SBL_AE - 1][clmr_ap_type][0]))) {
            type |= SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_SBL;
        }

        ret = shdisp_clmr_vsp_on_plus(type);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_vsp_on_plus Error!!!\n");
            return ret;
        }
    } else {
        ret = shdisp_clmr_vsp_on();
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_vsp_on Error!!!\n");
            return ret;
        }
    }

    clmr_pic_adj.mode = pic_adj->mode;

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_api_get_trv_info                                              */
/* ------------------------------------------------------------------------- */
struct shdisp_clmr_trv_info *shdisp_clmr_api_get_trv_info(void)
{
    return &clmr_trv_info;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_api_set_trv_param                                             */
/* ------------------------------------------------------------------------- */
int shdisp_clmr_api_set_trv_param(struct shdisp_trv_param *trv_param)
{
    int ret = SHDISP_RESULT_SUCCESS;
    const unsigned char *now;
    const unsigned char *set;
    unsigned char off[NUM_PIC_ADJ_MATRIX];
    unsigned char type = SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_OFF;
    int data_chg = 0;
    int param_chg = 0;
    int status_chg = 0;
    int new_status = SHDISP_CLMR_TRV_OFF;
    int i;

    SHDISP_TRACE("in request = %d.\n", trv_param->request);

    if (trv_param->request == SHDISP_LCDC_TRV_REQ_START) {
        if (clmr_trv_info.data_size == 0) {
            SHDISP_ERR("<INVALID_VALUE> TRV Data Not Set.\n");
            return SHDISP_RESULT_FAILURE;
        }
        if (clmr_trv_info.status == SHDISP_CLMR_TRV_ON) {
            SHDISP_ERR("<INVALID_VALUE> TRV Already ON.\n");
            return SHDISP_RESULT_FAILURE;
        }
        status_chg = 1;
        new_status = SHDISP_CLMR_TRV_ON;
    } else if (trv_param->request == SHDISP_LCDC_TRV_REQ_SET_PARAM) {
        if ((trv_param->strength < SHDISP_LCDC_TRV_STRENGTH_00) ||
            (trv_param->strength >= NUM_SHDISP_LCDC_TRV_STRENGTH)) {
            SHDISP_ERR("<INVALID_VALUE> strength = %d.\n", trv_param->strength);
            return SHDISP_RESULT_FAILURE;
        }
        if ((trv_param->adjust < SHDISP_LCDC_TRV_ADJUST_00) || (trv_param->adjust >= NUM_SHDISP_LCDC_TRV_ADJUST)) {
            SHDISP_ERR("<INVALID_VALUE> adjust = %d.\n", trv_param->adjust);
            return SHDISP_RESULT_FAILURE;
        }
        if ((trv_param->data == NULL) &&
            (trv_param->strength == clmr_trv_info.strength) &&
            (trv_param->adjust == clmr_trv_info.adjust)) {
            SHDISP_DEBUG("<INVALID_VALUE> Same Parameter.\n");
            return SHDISP_RESULT_SUCCESS;
        }
        if (clmr_trv_info.status == SHDISP_CLMR_TRV_ON) {
            if (trv_param->data != NULL) {
                data_chg = 1;
                SHDISP_DEBUG("data_chg strength = %d adjust = %d\n", trv_param->strength, trv_param->adjust);
            } else {
                param_chg = 1;
                SHDISP_DEBUG("param_chg strength = %d adjust = %d\n", trv_param->strength, trv_param->adjust);
            }
        }

        clmr_trv_info.strength  = trv_param->strength;
        clmr_trv_info.adjust    = trv_param->adjust;
    } else if (trv_param->request == SHDISP_LCDC_TRV_REQ_STOP) {
        if (clmr_trv_info.status == SHDISP_CLMR_TRV_OFF) {
            SHDISP_ERR("<INVALID_VALUE> TRV Already OFF.\n");
            return SHDISP_RESULT_FAILURE;
        }
        status_chg = 1;
        new_status = SHDISP_CLMR_TRV_OFF;
    } else {
        SHDISP_ERR("<INVALID_VALUE> trv_param->request(%d).\n", trv_param->request);
        return SHDISP_RESULT_FAILURE;
    }

    if (shdisp_pm_is_clmr_on() != SHDISP_DEV_STATE_ON) {
        if (trv_param->request == SHDISP_LCDC_TRV_REQ_START) {
            clmr_trv_info.status = SHDISP_CLMR_TRV_ON;
        } else if (trv_param->request == SHDISP_LCDC_TRV_REQ_SET_PARAM) {
            clmr_trv_info.strength  = trv_param->strength;
            clmr_trv_info.adjust    = trv_param->adjust;
        } else if (trv_param->request == SHDISP_LCDC_TRV_REQ_STOP) {
            clmr_trv_info.status = SHDISP_CLMR_TRV_OFF;
        }
        SHDISP_DEBUG("CLMR IS NOT ACTIVE.\n");
        return SHDISP_RESULT_SUCCESS;
    }

    if (status_chg) {
        now = shdisp_clmr_get_pic_adj_matrix(clmr_trv_info.status, &clmr_dbc, &clmr_pic_adj, clmr_ap_type);
        SHDISP_DEBUG("now = SVCT(%d), HSV(%d), PCA(%d), CPF(%d), AE(%d), SBL(%d), SMITE(%d), TRV(%d)\n",
                         now[PIC_ADJ_MATRIX_SVCT],  now[PIC_ADJ_MATRIX_HSV], now[PIC_ADJ_MATRIX_PCA],
                         now[PIC_ADJ_MATRIX_CPF],   now[PIC_ADJ_MATRIX_AE],  now[PIC_ADJ_MATRIX_SBL],
                         now[PIC_ADJ_MATRIX_SMITE], now[PIC_ADJ_MATRIX_TRV]);
        set = shdisp_clmr_get_pic_adj_matrix(new_status, &clmr_dbc, &clmr_pic_adj, clmr_ap_type);
        SHDISP_DEBUG("set = SVCT(%d), HSV(%d), PCA(%d), CPF(%d), AE(%d), SBL(%d), SMITE(%d), TRV(%d)\n",
                         set[PIC_ADJ_MATRIX_SVCT],  set[PIC_ADJ_MATRIX_HSV], set[PIC_ADJ_MATRIX_PCA],
                         set[PIC_ADJ_MATRIX_CPF],   set[PIC_ADJ_MATRIX_AE],  set[PIC_ADJ_MATRIX_SBL],
                         set[PIC_ADJ_MATRIX_SMITE], set[PIC_ADJ_MATRIX_TRV]);

        for (i = 0; i < NUM_PIC_ADJ_MATRIX; i++) {
            off[i] = ((now[i] != 0) && (set[i] == 0)) ? now[i] : 0;
        }
        SHDISP_DEBUG("off = SVCT(%d), HSV(%d), PCA(%d), CPF(%d), AE(%d), SBL(%d), SMITE(%d), TRV(%d)\n",
                         off[PIC_ADJ_MATRIX_SVCT],  off[PIC_ADJ_MATRIX_HSV], off[PIC_ADJ_MATRIX_PCA],
                         off[PIC_ADJ_MATRIX_CPF],   off[PIC_ADJ_MATRIX_AE],  off[PIC_ADJ_MATRIX_SBL],
                         off[PIC_ADJ_MATRIX_SMITE], off[PIC_ADJ_MATRIX_TRV]);

        if (off[PIC_ADJ_MATRIX_TRV]) {
            ret = shdisp_clmr_sqe_trv_off();
            if (ret != SHDISP_RESULT_SUCCESS) {
                SHDISP_ERR("shdisp_clmr_sqe_trv_off Error!!!\n");
                return ret;
            }
        }

        ret = shdisp_clmr_sqe_pic_adj_off(off);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_sqe_pic_adj_off Error!!!\n");
            return ret;
        }

        if (set[PIC_ADJ_MATRIX_SMITE]) {
            ret = shdisp_clmr_sqe_smite_on(smite_matrix[set[PIC_ADJ_MATRIX_SMITE]], clmr_ap_type);
            if (ret != SHDISP_RESULT_SUCCESS) {
                SHDISP_ERR("shdisp_clmr_sqe_smite_on Error!!!\n");
                return ret;
            }
        }

        if (set[PIC_ADJ_MATRIX_SBL] == SHDISP_CLMR_SBL_ACC) {
            ret = shdisp_clmr_sqe_sbl_on(set[PIC_ADJ_MATRIX_SBL], clmr_ap_type);
            if (ret != SHDISP_RESULT_SUCCESS) {
                SHDISP_ERR("shdisp_clmr_sqe_sbl_on Error!!!\n");
                return ret;
            }
        }

        ret = shdisp_clmr_sqe_pic_adj_on(&clmr_pic_adj, clmr_ap_type, set);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_sqe_pic_adj_on Error!!!\n");
            return ret;
        }

        if (set[PIC_ADJ_MATRIX_TRV]) {
            ret = shdisp_clmr_ewb_cross_lut_tbl(SHDISP_MAIN_DISP_PIC_ADJ_MODE_00, SHDISP_LCDC_PIC_ADJ_AP_NORMAL);
        } else {
            ret = shdisp_clmr_ewb_cross_lut_tbl(clmr_pic_adj.mode, clmr_ap_type);
        }
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_ewb_cross_lut_tbl Error!!!\n");
            return ret;
        }
        ret = shdisp_clmr_sqe_ewb_lut_chg();
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_sqe_ewb_lut_chg Error!!!\n");
            return ret;
        }

        if (set[PIC_ADJ_MATRIX_TRV]) {
#if defined(CONFIG_SHDISP_PANEL_RYOMA)  || defined(CONFIG_SHDISP_PANEL_GEMINI)
            shdisp_panel_API_request_RateCtrl(1, SHDISP_PANEL_RATE_60_0, SHDISP_PANEL_RATE_60_0);
#endif
            ret = shdisp_clmr_sqe_trv_on();
            if (ret != SHDISP_RESULT_SUCCESS) {
                SHDISP_ERR("shdisp_clmr_sqe_trv_on Error!!!\n");
                return ret;
            }
        }

        if (set[PIC_ADJ_MATRIX_TRV]) {
            ret = shdisp_clmr_vsp_on_plus(SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_TRV);
        } else {
            if (set[PIC_ADJ_MATRIX_CPF] &&
                (clmr_pic_adj.mode != SHDISP_MAIN_DISP_PIC_ADJ_MODE_00) &&
                cpf_on_param[clmr_pic_adj.mode - 1][clmr_ap_type][0]) {
                type |= SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_CPF;
            }
            if ((set[PIC_ADJ_MATRIX_SBL] == SHDISP_CLMR_SBL_ACC)
             || ((set[PIC_ADJ_MATRIX_SBL] == SHDISP_CLMR_SBL_AE) &&
                 (sbl_on_param[SHDISP_CLMR_SBL_AE - 1][clmr_ap_type][0]))) {
                type |= SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_SBL;
            }

            ret = shdisp_clmr_vsp_on_plus(type);
            if (ret != SHDISP_RESULT_SUCCESS) {
                SHDISP_ERR("shdisp_clmr_vsp_on_plus Error!!!\n");
                return ret;
            }

#if defined(CONFIG_SHDISP_PANEL_RYOMA)  || defined(CONFIG_SHDISP_PANEL_GEMINI)
            shdisp_panel_API_request_RateCtrl(1, SHDISP_PANEL_RATE_60_0, SHDISP_PANEL_RATE_1);
#endif
        }
    } else {
        if (data_chg) {
            ret = shdisp_clmr_sqe_trv_img_chg();
            if (ret != SHDISP_RESULT_SUCCESS) {
                SHDISP_ERR("shdisp_clmr_sqe_trv_img_chg(%d) Error!!!\n", data_chg);
                return ret;
            }
        } else if (param_chg) {
            ret = shdisp_clmr_sqe_trv_lut_chg();
            if (ret != SHDISP_RESULT_SUCCESS) {
                SHDISP_ERR("shdisp_clmr_sqe_trv_lut_chg(%d) Error!!!\n", data_chg);
                return ret;
            }

            ret = shdisp_clmr_vsp_on();
            if (ret != SHDISP_RESULT_SUCCESS) {
                SHDISP_ERR("shdisp_clmr_vsp_on Error!!!\n");
                return ret;
            }
        }
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_api_set_dbc_param                                             */
/* ------------------------------------------------------------------------- */
int shdisp_clmr_api_set_dbc_param(struct shdisp_main_dbc *dbc)
{
    int ret = SHDISP_RESULT_SUCCESS;
    const unsigned char *now;
    const unsigned char *set;
    unsigned char type = SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_OFF;

    SHDISP_TRACE("in\n");
    SHDISP_DEBUG("now mode = %d auto_mode = %d", clmr_dbc.mode, clmr_dbc.auto_mode);
    SHDISP_DEBUG("req mode = %d auto_mode = %d", dbc->mode, dbc->auto_mode);

    if ((clmr_dbc.mode == dbc->mode) && (clmr_dbc.auto_mode == dbc->auto_mode)) {
        SHDISP_DEBUG("<INVALID_VALUE> Same Parameter.\n");
        return SHDISP_RESULT_SUCCESS;
    }

    if (shdisp_pm_is_clmr_on() != SHDISP_DEV_STATE_ON) {
        clmr_dbc.mode      = dbc->mode;
        clmr_dbc.auto_mode = dbc->auto_mode;
        SHDISP_DEBUG("CLMR IS NOT ACTIVE.\n");
        return SHDISP_RESULT_SUCCESS;
    }

    if (clmr_trv_info.status == SHDISP_CLMR_TRV_ON) {
        clmr_dbc.mode      = dbc->mode;
        clmr_dbc.auto_mode = dbc->auto_mode;
        SHDISP_DEBUG("TRV ON.\n");
        return SHDISP_RESULT_SUCCESS;
    }

    if (clmr_need_dbc_startup) {
        clmr_dbc.mode      = dbc->mode;
        clmr_dbc.auto_mode = dbc->auto_mode;
        SHDISP_DEBUG("startup not yet.\n");
        return SHDISP_RESULT_SUCCESS;
    }

    now = shdisp_clmr_get_pic_adj_matrix(clmr_trv_info.status, &clmr_dbc, &clmr_pic_adj, clmr_ap_type);
    SHDISP_DEBUG("now = SVCT(%d), HSV(%d), PCA(%d), CPF(%d), AE(%d), SBL(%d), SMITE(%d), TRV(%d)\n",
                     now[PIC_ADJ_MATRIX_SVCT],  now[PIC_ADJ_MATRIX_HSV], now[PIC_ADJ_MATRIX_PCA],
                     now[PIC_ADJ_MATRIX_CPF],   now[PIC_ADJ_MATRIX_AE],  now[PIC_ADJ_MATRIX_SBL],
                     now[PIC_ADJ_MATRIX_SMITE], now[PIC_ADJ_MATRIX_TRV]);
    set = shdisp_clmr_get_pic_adj_matrix(clmr_trv_info.status, dbc, &clmr_pic_adj, clmr_ap_type);
    SHDISP_DEBUG("set = SVCT(%d), HSV(%d), PCA(%d), CPF(%d), AE(%d), SBL(%d), SMITE(%d), TRV(%d)\n",
                     set[PIC_ADJ_MATRIX_SVCT],  set[PIC_ADJ_MATRIX_HSV], set[PIC_ADJ_MATRIX_PCA],
                     set[PIC_ADJ_MATRIX_CPF],   set[PIC_ADJ_MATRIX_AE],  set[PIC_ADJ_MATRIX_SBL],
                     set[PIC_ADJ_MATRIX_SMITE], set[PIC_ADJ_MATRIX_TRV]);

    if (now[PIC_ADJ_MATRIX_SBL] && !set[PIC_ADJ_MATRIX_SBL]) {
        ret = shdisp_clmr_sqe_sbl_off();
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_sqe_sbl_off Error!!!\n");
            return ret;
        }
    }

    if (now[PIC_ADJ_MATRIX_SMITE] != set[PIC_ADJ_MATRIX_SMITE]) {
        if ((!now[PIC_ADJ_MATRIX_SMITE]) && (set[PIC_ADJ_MATRIX_SMITE])) {
            ret = shdisp_clmr_sqe_smite_on(smite_matrix[set[PIC_ADJ_MATRIX_SMITE]], clmr_ap_type);
            if (ret != SHDISP_RESULT_SUCCESS) {
                SHDISP_ERR("shdisp_clmr_sqe_smite_on Error!!!\n");
                return ret;
            }
        } else if ((now[PIC_ADJ_MATRIX_SMITE]) && (!set[PIC_ADJ_MATRIX_SMITE])) {
            ret = shdisp_clmr_sqe_smite_off();
            if (ret != SHDISP_RESULT_SUCCESS) {
                SHDISP_ERR("shdisp_clmr_sqe_smite_off Error!!!\n");
                return ret;
            }
        } else {
            ret = shdisp_clmr_sqe_smite_mode_chg(smite_matrix[set[PIC_ADJ_MATRIX_SMITE]]);
            if (ret != SHDISP_RESULT_SUCCESS) {
                SHDISP_ERR("shdisp_clmr_sqe_smite_mode_chg Error!!!\n");
                return ret;
            }
        }
    }

    if (!now[PIC_ADJ_MATRIX_SBL] && set[PIC_ADJ_MATRIX_SBL]) {
        ret = shdisp_clmr_sqe_sbl_on(set[PIC_ADJ_MATRIX_SBL], clmr_ap_type);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_sqe_sbl_on Error!!!\n");
            return ret;
        }
    }

    if ((now[PIC_ADJ_MATRIX_SBL] && !set[PIC_ADJ_MATRIX_SBL])
     || (!now[PIC_ADJ_MATRIX_SBL] && set[PIC_ADJ_MATRIX_SBL])) {

        if (set[PIC_ADJ_MATRIX_CPF] &&
            (clmr_pic_adj.mode != SHDISP_MAIN_DISP_PIC_ADJ_MODE_00) &&
            cpf_on_param[clmr_pic_adj.mode - 1][clmr_ap_type][0]) {
            type |= SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_CPF;
        }
        if ((set[PIC_ADJ_MATRIX_SBL] == SHDISP_CLMR_SBL_ACC)
         || ((set[PIC_ADJ_MATRIX_SBL] == SHDISP_CLMR_SBL_AE) &&
             (sbl_on_param[SHDISP_CLMR_SBL_AE - 1][clmr_ap_type][0]))) {
            type |= SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_SBL;
        }

        ret = shdisp_clmr_vsp_on_plus(type);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_vsp_on_plus Error!!!\n");
            return ret;
        }
    } else {
        ret = shdisp_clmr_vsp_on();
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_vsp_on Error!!!\n");
            return ret;
        }
    }

    clmr_dbc.mode      = dbc->mode;
    clmr_dbc.auto_mode = dbc->auto_mode;

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_api_set_ae_param                                              */
/* ------------------------------------------------------------------------- */
int shdisp_clmr_api_set_ae_param(struct shdisp_main_ae *ae)
{
    int ret = SHDISP_RESULT_SUCCESS;
    const unsigned char *now;

    SHDISP_TRACE("in\n");
    SHDISP_DEBUG("now time = %d", clmr_ae.time);
    SHDISP_DEBUG("req time = %d", ae->time);

    if (ae->time > SHDISP_MAIN_DISP_AE_TIME_MORNING) {
        SHDISP_ERR("<INVALID_VALUE> time = %d.\n", ae->time);
        return SHDISP_RESULT_FAILURE;
    }
    if (clmr_ae.time == ae->time) {
        SHDISP_DEBUG("<INVALID_VALUE> Same Parameter.\n");
        return SHDISP_RESULT_SUCCESS;
    }

    if (shdisp_pm_is_clmr_on() != SHDISP_DEV_STATE_ON) {
        clmr_ae.time = ae->time;
        SHDISP_DEBUG("CLMR IS NOT ACTIVE.\n");
        return SHDISP_RESULT_SUCCESS;
    }

    now = shdisp_clmr_get_pic_adj_matrix(clmr_trv_info.status, &clmr_dbc, &clmr_pic_adj, clmr_ap_type);
    SHDISP_DEBUG("now = SVCT(%d), HSV(%d), PCA(%d), CPF(%d), AE(%d), SBL(%d), SMITE(%d), TRV(%d)\n",
                     now[PIC_ADJ_MATRIX_SVCT],  now[PIC_ADJ_MATRIX_HSV], now[PIC_ADJ_MATRIX_PCA],
                     now[PIC_ADJ_MATRIX_CPF],   now[PIC_ADJ_MATRIX_AE],  now[PIC_ADJ_MATRIX_SBL],
                     now[PIC_ADJ_MATRIX_SMITE], now[PIC_ADJ_MATRIX_TRV]);

    if (now[PIC_ADJ_MATRIX_AE]) {
        ret = shdisp_clmr_sqe_ae_time_set(ae->time);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_sqe_ae_time_set Error!!!\n");
            return ret;
        }
    }
    clmr_ae.time = ae->time;

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_api_set_pic_adj_ap_type                                       */
/* ------------------------------------------------------------------------- */
int shdisp_clmr_api_set_pic_adj_ap_type(unsigned short ap_type)
{
    int ret = SHDISP_RESULT_SUCCESS;
    const unsigned char *now;
    unsigned char set[NUM_PIC_ADJ_MATRIX];
    unsigned char off[NUM_PIC_ADJ_MATRIX];
    int i;
    unsigned char type = SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_OFF;
    int lpmc_setting_chg = 0;

    SHDISP_TRACE("in\n");
    SHDISP_DEBUG("now ap_type = %d", clmr_ap_type);
    SHDISP_DEBUG("req ap_type = %d", ap_type);

#ifdef KERNEL_CALL_PIC_ADJ_MDP
    ret = shdisp_clmr_set_pic_adj_data(clmr_pic_adj.mode, ap_type);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_set_pic_adj_data Error!!!\n");
        return ret;
    }
#endif

    if (ap_type >= NUM_SHDISP_LCDC_PIC_ADJ_AP) {
        SHDISP_ERR("<INVALID_VALUE> ap_type = %d.\n", ap_type);
        return SHDISP_RESULT_FAILURE;
    }
    if (clmr_ap_type == ap_type) {
        SHDISP_DEBUG("<INVALID_VALUE> Same Parameter.\n");
        return SHDISP_RESULT_SUCCESS;
    }

    if (shdisp_pm_is_clmr_on() != SHDISP_DEV_STATE_ON) {
        clmr_ap_type = ap_type;
        SHDISP_DEBUG("CLMR IS NOT ACTIVE.\n");
        return SHDISP_RESULT_SUCCESS;
    }

    if (clmr_pic_adj.mode == SHDISP_MAIN_DISP_PIC_ADJ_MODE_00) {
        clmr_ap_type = ap_type;
        SHDISP_DEBUG("PIC_ADJ OFF.\n");
        return SHDISP_RESULT_SUCCESS;
    }

#if defined(CONFIG_SHDISP_PANEL_ANDY)
    if ((clmr_ap_type == SHDISP_LCDC_PIC_ADJ_AP_1SEG)
     || (clmr_ap_type == SHDISP_LCDC_PIC_ADJ_AP_FULLSEG)
     || (clmr_ap_type == SHDISP_LCDC_PIC_ADJ_AP_TMM)) {
        if ((ap_type == SHDISP_LCDC_PIC_ADJ_AP_NORMAL)
         || (ap_type == SHDISP_LCDC_PIC_ADJ_AP_CAM)) {
            shdisp_clmr_api_rate_check_mode_ctrl(SHDISP_CLMR_RATE_CHECK_MODE_ON);
        }
    } else {
        if ((ap_type == SHDISP_LCDC_PIC_ADJ_AP_1SEG)
         || (ap_type == SHDISP_LCDC_PIC_ADJ_AP_FULLSEG)
         || (ap_type == SHDISP_LCDC_PIC_ADJ_AP_TMM)) {
            shdisp_clmr_api_rate_check_mode_ctrl(SHDISP_CLMR_RATE_CHECK_MODE_OFF);
        }
    }
#endif /* CONFIG_SHDISP_PANEL_ANDY */

    now = shdisp_clmr_get_pic_adj_matrix(clmr_trv_info.status, &clmr_dbc, &clmr_pic_adj, clmr_ap_type);
    SHDISP_DEBUG("now = SVCT(%d), HSV(%d), PCA(%d), CPF(%d), AE(%d), SBL(%d), SMITE(%d), TRV(%d)\n",
                     now[PIC_ADJ_MATRIX_SVCT],  now[PIC_ADJ_MATRIX_HSV], now[PIC_ADJ_MATRIX_PCA],
                     now[PIC_ADJ_MATRIX_CPF],   now[PIC_ADJ_MATRIX_AE],  now[PIC_ADJ_MATRIX_SBL],
                     now[PIC_ADJ_MATRIX_SMITE], now[PIC_ADJ_MATRIX_TRV]);
    memcpy(set, shdisp_clmr_get_pic_adj_matrix(clmr_trv_info.status, &clmr_dbc, &clmr_pic_adj, ap_type),
                                                                                    NUM_PIC_ADJ_MATRIX);
    SHDISP_DEBUG("set = SVCT(%d), HSV(%d), PCA(%d), CPF(%d), AE(%d), SBL(%d), SMITE(%d), TRV(%d)\n",
                     set[PIC_ADJ_MATRIX_SVCT],  set[PIC_ADJ_MATRIX_HSV], set[PIC_ADJ_MATRIX_PCA],
                     set[PIC_ADJ_MATRIX_CPF],   set[PIC_ADJ_MATRIX_AE],  set[PIC_ADJ_MATRIX_SBL],
                     set[PIC_ADJ_MATRIX_SMITE], set[PIC_ADJ_MATRIX_TRV]);
    for (i = 0; i < NUM_PIC_ADJ_MATRIX; i++) {
        off[i] = ((now[i] != 0) && (set[i] == 0)) ? now[i] : 0;
    }
    SHDISP_DEBUG("off = SVCT(%d), HSV(%d), PCA(%d), CPF(%d), AE(%d), SBL(%d), SMITE(%d), TRV(%d)\n",
                     off[PIC_ADJ_MATRIX_SVCT],  off[PIC_ADJ_MATRIX_HSV], off[PIC_ADJ_MATRIX_PCA],
                     off[PIC_ADJ_MATRIX_CPF],   off[PIC_ADJ_MATRIX_AE],  off[PIC_ADJ_MATRIX_SBL],
                     off[PIC_ADJ_MATRIX_SMITE], off[PIC_ADJ_MATRIX_TRV]);

    ret = shdisp_clmr_sqe_pic_adj_off(off);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_sqe_pic_adj_off Error!!!\n");
        return ret;
    }

    if (set[PIC_ADJ_MATRIX_PCA] && now[PIC_ADJ_MATRIX_PCA]) {
        set[PIC_ADJ_MATRIX_PCA] = 0;
    }
    if (set[PIC_ADJ_MATRIX_AE] && now[PIC_ADJ_MATRIX_AE]) {
        set[PIC_ADJ_MATRIX_AE] = 0;
    }

    if (now[PIC_ADJ_MATRIX_SBL] == SHDISP_CLMR_SBL_ACC)
        set[PIC_ADJ_MATRIX_SBL] = SHDISP_CLMR_SBL_AE;

    if (smite_matrix[set[PIC_ADJ_MATRIX_SMITE]]) {
        ret = shdisp_clmr_sqe_smite_lpmc_setting_chg(ap_type);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_sqe_smite_lpmc_setting_chg Error!!!\n");
            return ret;
        }
        lpmc_setting_chg = 1;
    }

    if (clmr_trv_info.status == SHDISP_CLMR_TRV_ON) {
        if (lpmc_setting_chg) {
            ret = shdisp_clmr_vsp_on();
            if (ret != SHDISP_RESULT_SUCCESS) {
                SHDISP_ERR("shdisp_clmr_vsp_on Error!!!\n");
                return ret;
            }
        }
        clmr_ap_type = ap_type;
        SHDISP_DEBUG("TRV ON.\n");
        return SHDISP_RESULT_SUCCESS;
    }

    ret = shdisp_clmr_sqe_pic_adj_on(&clmr_pic_adj, ap_type, set);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_sqe_pic_adj_on Error!!!\n");
        return ret;
    }

    ret = shdisp_clmr_ewb_cross_lut_tbl(clmr_pic_adj.mode, ap_type);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_ewb_cross_lut_tbl Error!!!\n");
        return ret;
    }
    ret = shdisp_clmr_sqe_ewb_lut_chg();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_api_set_ewb_tbl Error!!!\n");
        return ret;
    }

    if ((set[PIC_ADJ_MATRIX_CPF] != off[PIC_ADJ_MATRIX_CPF]) ||
        (set[PIC_ADJ_MATRIX_SBL] != off[PIC_ADJ_MATRIX_SBL])) {

        if (set[PIC_ADJ_MATRIX_CPF] &&
            (clmr_pic_adj.mode != SHDISP_MAIN_DISP_PIC_ADJ_MODE_00) &&
            cpf_on_param[clmr_pic_adj.mode - 1][ap_type][0]) {
            type |= SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_CPF;
        }
        if ((set[PIC_ADJ_MATRIX_SBL] == SHDISP_CLMR_SBL_ACC)
         || ((set[PIC_ADJ_MATRIX_SBL] == SHDISP_CLMR_SBL_AE) && (sbl_on_param[SHDISP_CLMR_SBL_AE - 1][ap_type][0]))) {
            type |= SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF_SBL;
        }

        ret = shdisp_clmr_vsp_on_plus(type);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_vsp_on_plus Error!!!\n");
            return ret;
        }
    } else {
        ret = shdisp_clmr_vsp_on();
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_vsp_on Error!!!\n");
            return ret;
        }
    }

    clmr_ap_type = ap_type;

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sqe_pic_adj_on                                                */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sqe_pic_adj_on(struct shdisp_main_pic_adj *pic_adj, unsigned short ap_type,
                                                                           const unsigned char *set)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in mode = %d.\n", pic_adj->mode);

    if ((pic_adj->mode < SHDISP_MAIN_DISP_PIC_ADJ_MODE_00) || (pic_adj->mode >= NUM_SHDISP_MAIN_DISP_PIC_ADJ_MODE)) {
        SHDISP_ERR("<INVALID_VALUE> mode = %d.\n", pic_adj->mode);
        return SHDISP_RESULT_FAILURE;
    }
    if (ap_type >= NUM_SHDISP_LCDC_PIC_ADJ_AP) {
        SHDISP_ERR("<INVALID_VALUE> ap_type = %d.\n", ap_type);
        return SHDISP_RESULT_FAILURE;
    }
    SHDISP_DEBUG("set = SVCT(%d), HSV(%d), PCA(%d), CPF(%d), AE(%d), SBL(%d), SMITE(%d), TRV(%d)\n",
                     set[PIC_ADJ_MATRIX_SVCT],  set[PIC_ADJ_MATRIX_HSV], set[PIC_ADJ_MATRIX_PCA],
                     set[PIC_ADJ_MATRIX_CPF],   set[PIC_ADJ_MATRIX_AE],  set[PIC_ADJ_MATRIX_SBL],
                     set[PIC_ADJ_MATRIX_SMITE], set[PIC_ADJ_MATRIX_TRV]);

    if (set[PIC_ADJ_MATRIX_SBL] == SHDISP_CLMR_SBL_AE) {
        ret = shdisp_clmr_sqe_sbl_on(set[PIC_ADJ_MATRIX_SBL], ap_type);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_sqe_sbl_on Error!!!\n");
            return ret;
        }
    }

    if (set[PIC_ADJ_MATRIX_SVCT]) {
        ret = shdisp_clmr_sqe_svct_on(pic_adj->mode, ap_type);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_sqe_svct_on Error!!!\n");
            return ret;
        }
    }

    if (set[PIC_ADJ_MATRIX_HSV]) {
        ret = shdisp_clmr_sqe_hsv_on(pic_adj->mode, ap_type);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_sqe_hsv_on Error!!!\n");
            return ret;
        }
    }

    if (set[PIC_ADJ_MATRIX_PCA]) {
        ret = shdisp_clmr_sqe_pca_on(pic_adj->mode, ap_type);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_sqe_pca_on Error!!!\n");
            return ret;
        }
    }

    if (set[PIC_ADJ_MATRIX_CPF]) {
        ret = shdisp_clmr_sqe_cpf_on(pic_adj->mode, ap_type);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_sqe_cpf_on Error!!!\n");
            return ret;
        }
    }

    if (set[PIC_ADJ_MATRIX_AE]) {
        ret = shdisp_clmr_sqe_ae_on();
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_sqe_ae_on Error!!!\n");
            return ret;
        }
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sqe_pic_adj_off                                               */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sqe_pic_adj_off(const unsigned char *off)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");
    SHDISP_DEBUG("off = SVCT(%d), HSV(%d), PCA(%d), CPF(%d), AE(%d), SBL(%d), SMITE(%d), TRV(%d)\n",
                     off[PIC_ADJ_MATRIX_SVCT],  off[PIC_ADJ_MATRIX_HSV], off[PIC_ADJ_MATRIX_PCA],
                     off[PIC_ADJ_MATRIX_CPF],   off[PIC_ADJ_MATRIX_AE],  off[PIC_ADJ_MATRIX_SBL],
                     off[PIC_ADJ_MATRIX_SMITE], off[PIC_ADJ_MATRIX_TRV]);

    if (off[PIC_ADJ_MATRIX_SBL]) {
        ret = shdisp_clmr_sqe_sbl_off();
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_sqe_sbl_off Error!!!\n");
            return ret;
        }
    }

    if (off[PIC_ADJ_MATRIX_SMITE]) {
        ret = shdisp_clmr_sqe_smite_off();
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_sqe_smite_off Error!!!\n");
            return ret;
        }
    }

    if (off[PIC_ADJ_MATRIX_SVCT]) {
        ret = shdisp_clmr_sqe_svct_off();
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_sqe_svct_off Error!!!\n");
            return ret;
        }
    }

    if (off[PIC_ADJ_MATRIX_HSV]) {
        ret = shdisp_clmr_sqe_hsv_off();
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_sqe_hsv_off Error!!!\n");
            return ret;
        }
    }

    if (off[PIC_ADJ_MATRIX_PCA]) {
        ret = shdisp_clmr_sqe_pca_off();
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_sqe_pca_off Error!!!\n");
            return ret;
        }
    }

    if (off[PIC_ADJ_MATRIX_CPF]) {
        ret = shdisp_clmr_sqe_cpf_off();
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_sqe_cpf_off Error!!!\n");
            return ret;
        }
    }

    if (off[PIC_ADJ_MATRIX_AE]) {
        ret = shdisp_clmr_sqe_ae_off(SHDISP_CLMR_AE_OFF_WITH_HSV_PCA);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("shdisp_clmr_sqe_ae_off Error!!!\n");
            return ret;
        }
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sqe_trv_on                                                    */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sqe_trv_on(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    ret = shdisp_clmr_trv_write_texture();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_trv_write_texture Error!!!\n");
        return ret;
    }

    ret = shdisp_clmr_trv_initial_setting();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_trv_initial_setting Error!!!\n");
        return ret;
    }

    ret = shdisp_clmr_trv_lut_write();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_trv_lut_write Error!!!\n");
        return ret;
    }

    ret = shdisp_clmr_trv_param_set();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_trv_param_set Error!!!\n");
        return ret;
    }

    clmr_trv_info.status = SHDISP_CLMR_TRV_ON;
    ret = shdisp_clmr_trv_on();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_trv_on Error!!!\n");
        return ret;
    }

#ifdef CONFIG_TOUCHSCREEN_SHTPS
    ret = msm_tps_set_veilview_state_on();
    if (ret) {
        SHDISP_ERR("msm_tps_set_veilview_state_on Error!!!\n");
    }
#endif /* CONFIG_TOUCHSCREEN_SHTPS */

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sqe_trv_off                                                   */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sqe_trv_off(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    clmr_trv_info.status = SHDISP_CLMR_TRV_OFF;
    ret = shdisp_clmr_trv_off();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_trv_off Error!!!\n");
        return ret;
    }

#ifdef CONFIG_TOUCHSCREEN_SHTPS
    ret = msm_tps_set_veilview_state_off();
    if (ret) {
        SHDISP_ERR("msm_tps_set_veilview_state_off Error!!!\n");
    }
#endif /* CONFIG_TOUCHSCREEN_SHTPS */

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sqe_trv_lut_chg                                               */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sqe_trv_lut_chg(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    ret = shdisp_clmr_trv_lut_write();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_trv_lut_write Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sqe_trv_img_chg                                               */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sqe_trv_img_chg(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    ret = shdisp_clmr_trv_filter_off();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_trv_filter_off Error!!!\n");
        return ret;
    }

    ret = shdisp_clmr_vsp_on();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_vsp_on Error!!!\n");
        return ret;
    }

    ret = shdisp_clmr_trv_write_texture();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_trv_write_texture Error!!!\n");
        return ret;
    }

    ret = shdisp_clmr_trv_initial_setting();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_trv_initial_setting Error!!!\n");
        return ret;
    }

    ret = shdisp_clmr_trv_filter_on();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_trv_filter_on Error!!!\n");
        return ret;
    }

    ret = shdisp_clmr_vsp_on();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_vsp_on Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sqe_sbl_on                                                    */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sqe_sbl_on(unsigned char mode, unsigned short ap_type)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    ret = shdisp_clmr_sbl_set_luxmode(SHDISP_CLMR_FWCMD_LIGHTCTL_LUXMODE_SET_SBL);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_sbl_set_luxmode Error!!!\n");
        return ret;
    }

    ret = shdisp_clmr_sbl_on_setting(mode, ap_type);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_sbl_on_setting Error!!!\n");
        return ret;
    }

    ret = shdisp_clmr_sbl_on();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_sbl_on Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sqe_sbl_off                                                   */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sqe_sbl_off(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    ret = shdisp_clmr_sbl_off();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_sbl_off Error!!!\n");
        return ret;
    }

    ret = shdisp_clmr_sbl_set_luxmode(SHDISP_CLMR_FWCMD_LIGHTCTL_LUXMODE_SET_OFF);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_sbl_set_luxmode Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sqe_svct_on                                                   */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sqe_svct_on(unsigned short mode, unsigned short ap_type)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in mode = %d ap_type = %d\n", mode, ap_type);

    ret = shdisp_clmr_svct_on(mode, ap_type);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_svct_on Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sqe_svct_off                                                  */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sqe_svct_off(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    ret = shdisp_clmr_svct_off();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_svct_off Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sqe_hsv_on                                                    */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sqe_hsv_on(unsigned short mode, unsigned short ap_type)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in mode = %d ap_type = %d\n", mode, ap_type);

    ret = shdisp_clmr_hsv_param_set(mode, ap_type);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_hsv_param_set Error!!!\n");
        return ret;
    }

    ret = shdisp_clmr_hsv_on(mode, ap_type);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_hsv_on Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sqe_hsv_off                                                   */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sqe_hsv_off(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    ret = shdisp_clmr_hsv_off();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_hsv_off Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sqe_pca_on                                                    */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sqe_pca_on(unsigned short mode, unsigned short ap_type)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in mode = %d ap_type = %d\n", mode, ap_type);

    ret = shdisp_clmr_pca_config(mode, ap_type);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_pca_config Error!!!\n");
        return ret;
    }

    ret = shdisp_clmr_pca_on(mode, ap_type);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_pca_on Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sqe_pca_off                                                   */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sqe_pca_off(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    ret = shdisp_clmr_pca_off();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_pca_off Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sqe_cpf_on                                                    */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sqe_cpf_on(unsigned short mode, unsigned short ap_type)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in mode = %d ap_type = %d\n", mode, ap_type);

    ret = shdisp_clmr_cpf_lut_write(mode, ap_type);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_cpf_lut_write Error!!!\n");
        return ret;
    }

    ret = shdisp_clmr_cpf_param_set(mode, ap_type);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_cpf_param_set Error!!!\n");
        return ret;
    }

    ret = shdisp_clmr_cpf_on(mode);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_cpf_on Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sqe_cpf_off                                                   */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sqe_cpf_off(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    ret = shdisp_clmr_cpf_off();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_cpf_off Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sqe_cpf_mode_chg                                              */
/* ------------------------------------------------------------------------- */
int shdisp_clmr_sqe_cpf_mode_chg(unsigned short mode)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in mode = %d\n", mode);

    ret = shdisp_clmr_cpf_on(mode);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_cpf_on Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sqe_cpf_lut_chg                                               */
/* ------------------------------------------------------------------------- */
int shdisp_clmr_sqe_cpf_lut_chg(unsigned short mode, unsigned short ap_type)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in mode = %d ap_type = %d\n", mode, ap_type);

    ret = shdisp_clmr_cpf_lut_write(mode, ap_type);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_cpf_lut_write Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sqe_smite_on                                                  */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sqe_smite_on(unsigned short mode, unsigned short ap_type)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in mode = %d\n", mode);

    ret = shdisp_clmr_ae_param_set();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_ae_param_set Error!!!\n");
        return ret;
    }

    ret = shdisp_clmr_smite_on(mode, ap_type);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_smite_on Error!!!\n");
        return ret;
    }

    if ((mode == SHDISP_CLMR_FWCMD_SMITE_SET_MODE_DBC_ACC) || (mode == SHDISP_CLMR_FWCMD_SMITE_SET_MODE_ACC)) {
        shdisp_bdic_API_set_lux_mode_modify(SHDISP_CLMR_FWCMD_LIGHTCTL_LUXMODE_SET_SMITE,
                                            SHDISP_CLMR_FWCMD_LIGHTCTL_LUXMODE_SET_SMITE);
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sqe_smite_mode_chg                                            */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sqe_smite_mode_chg(unsigned short mode)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in mode = %d\n", mode);

    ret = shdisp_clmr_smite_mode_chg(mode);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_smite_mode_chg Error!!!\n");
        return ret;
    }

    if ((mode == SHDISP_CLMR_FWCMD_SMITE_SET_MODE_DBC_ACC) || (mode == SHDISP_CLMR_FWCMD_SMITE_SET_MODE_ACC)) {
        shdisp_bdic_API_set_lux_mode_modify(SHDISP_CLMR_FWCMD_LIGHTCTL_LUXMODE_SET_SMITE,
                                            SHDISP_CLMR_FWCMD_LIGHTCTL_LUXMODE_SET_SMITE);
    } else {
        shdisp_bdic_API_set_lux_mode_modify(SHDISP_CLMR_FWCMD_LIGHTCTL_LUXMODE_SET_OFF,
                                            SHDISP_CLMR_FWCMD_LIGHTCTL_LUXMODE_SET_SMITE);
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sqe_smite_off                                                 */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sqe_smite_off(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    shdisp_bdic_API_set_lux_mode_modify(SHDISP_CLMR_FWCMD_LIGHTCTL_LUXMODE_SET_OFF,
                                        SHDISP_CLMR_FWCMD_LIGHTCTL_LUXMODE_SET_SMITE);

    ret = shdisp_clmr_smite_off();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_smite_off Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sqe_smite_lpmc_setting_chg                                    */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sqe_smite_lpmc_setting_chg(unsigned short ap_type)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in ap_type = %d\n", ap_type);

    ret = shdisp_clmr_smite_lpmc_setting_chg(ap_type);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_smite_lpmc_setting_chg Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sqe_ewb_lut_chg                                               */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sqe_ewb_lut_chg(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    ret = shdisp_clmr_ewb_lut_write(&clmr_ewb_accu_cross, SHDISP_CLMR_EWB_LUT_NO_0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_ewb_lut_write Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sqe_ae_on                                                     */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sqe_ae_on(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    ret = shdisp_clmr_ae_param_set();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_ae_param_set Error!!!\n");
        return ret;
    }

    ret = shdisp_clmr_ae_time_set(clmr_ae.time);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_ae_time_set Error!!!\n");
        return ret;
    }

    ret = shdisp_clmr_ae_on();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_ae_on Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sqe_ae_off                                                    */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sqe_ae_off(unsigned char mode)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    ret = shdisp_clmr_ae_off(mode);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_ae_off Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sqe_ae_time_set                                               */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sqe_ae_time_set(unsigned char time)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in time = %d\n", time);

    ret = shdisp_clmr_ae_time_set(time);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_ae_time_set Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_trv_write_texture                                             */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_trv_write_texture(void)
{
    int ret = SHDISP_RESULT_SUCCESS;
    unsigned char wdata[4] = {0};

    SHDISP_TRACE("in\n");

    ret = shdisp_clmr_trv_mif_set(SHDISP_CLMR_TRV_BASE, clmr_trv_info.hw, clmr_trv_info.y_size);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_trv_mif_set Error!!!\n");
        return ret;
    }

    SHDISP_DEBUG("data_size = %d.\n", clmr_trv_info.data_size);
    SHDISP_DEBUG("data[0] = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x.\n",
                  clmr_trv_info.data[0],  clmr_trv_info.data[1],  clmr_trv_info.data[2],  clmr_trv_info.data[3],
                  clmr_trv_info.data[4],  clmr_trv_info.data[5],  clmr_trv_info.data[6],  clmr_trv_info.data[7],
                  clmr_trv_info.data[8],  clmr_trv_info.data[9],  clmr_trv_info.data[10], clmr_trv_info.data[11],
                  clmr_trv_info.data[12], clmr_trv_info.data[13], clmr_trv_info.data[14], clmr_trv_info.data[15]);
    SHDISP_DEBUG("data[%d] = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x.\n",
                  clmr_trv_info.data_size - 16,
                  clmr_trv_info.data[clmr_trv_info.data_size - 16], clmr_trv_info.data[clmr_trv_info.data_size - 15],
                  clmr_trv_info.data[clmr_trv_info.data_size - 14], clmr_trv_info.data[clmr_trv_info.data_size - 13],
                  clmr_trv_info.data[clmr_trv_info.data_size - 12], clmr_trv_info.data[clmr_trv_info.data_size - 11],
                  clmr_trv_info.data[clmr_trv_info.data_size - 10], clmr_trv_info.data[clmr_trv_info.data_size -  9],
                  clmr_trv_info.data[clmr_trv_info.data_size -  8], clmr_trv_info.data[clmr_trv_info.data_size -  7],
                  clmr_trv_info.data[clmr_trv_info.data_size -  6], clmr_trv_info.data[clmr_trv_info.data_size -  5],
                  clmr_trv_info.data[clmr_trv_info.data_size -  4], clmr_trv_info.data[clmr_trv_info.data_size -  3],
                  clmr_trv_info.data[clmr_trv_info.data_size -  2], clmr_trv_info.data[clmr_trv_info.data_size -  1]);
    ret = shdisp_SYS_clmr_sio_eDram_transfer(SHDISP_CLMR_EDRAM_C8, clmr_trv_info.data, clmr_trv_info.data_size);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("TRV TEXTURE write Error!!!\n");
        return ret;
    }

    shdisp_clmr_eDramPtr_rst_start();
    wdata[3] = 0x04;
    ret = shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_REG_ARMSETINT1, wdata, sizeof(wdata), NULL, 0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("CLMR REG (ARMSETINT1) set Error!!!\n");
        return SHDISP_RESULT_FAILURE;
    }

    ret = shdisp_clmr_api_wait4eDramPtr_rst_comp();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_api_wait4eDramPtr_rst_comp timeout Error!!!\n");
        return SHDISP_RESULT_FAILURE;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_trv_mif_set_                                                  */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_trv_mif_set_(unsigned short base, unsigned short hw, unsigned short y_size)
{
    unsigned char wdata[SHDISP_CLMR_FWCMD_TRV_MIF_SIZE];

    SHDISP_TRACE("in\n");

    wdata[0] = base & 0xFF;
    wdata[1] = (base >> 8) & 0xFF;
    wdata[2] = hw & 0xFF;
    wdata[3] = (hw >> 8) & 0xFF;
    wdata[4] = y_size & 0xFF;
    wdata[5] = (y_size >> 8) & 0xFF;

    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_TRV_MIF_WRITE, SHDISP_CLMR_FWCMD_TRV_MIF_SIZE, wdata);

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_trv_mif_set                                                   */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_trv_mif_set(unsigned short base, unsigned short hw, unsigned short y_size)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    if ((base >= 0x7FFF) || (hw >= 0x7FFF) || (y_size >= 0x7FFF)) {
        SHDISP_ERR("<INVALID_VALUE> base(%d) hw(%d) y_size(%d).\n", base, hw, y_size);
        return SHDISP_RESULT_FAILURE;
    }

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_trv_mif_set_(base, hw, y_size);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_trv_mif_set Error!!!\n");
        return ret;
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("TRV MIF set Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_trv_initial_setting_                                          */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_trv_initial_setting_(void)
{
    int ret = SHDISP_RESULT_SUCCESS;
    int size = ARRAY_SIZE(custom_trv_initial_setting);

    SHDISP_TRACE("in\n");

    custom_trv_initial_setting[0].data = clmr_trv_info.hw;
    custom_trv_initial_setting[1].data = 0x00010000 | (clmr_trv_info.hw & 0x00000FFF);
    SHDISP_DEBUG("SHDISP_CLMR_REG_TRVHW = 0x%08lx, SHDISP_CLMR_CUST_TRVRAMHW = 0x%08lx\n",
                    custom_trv_initial_setting[0].data, custom_trv_initial_setting[1].data);
    ret = shdisp_clmr_regsSetbyFW(custom_trv_initial_setting, size);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_regsSetbyFW(custom_trv_initial_setting) Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_trv_initial_setting                                           */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_trv_initial_setting(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_trv_initial_setting_();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_trv_initial_setting_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("TRV Initial Set Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_trv_lut_write_                                                */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_trv_lut_write_(void)
{
    int ret = SHDISP_RESULT_SUCCESS;
    int i = 0;
    const unsigned short *trv_lut_r;
    const unsigned short *trv_lut_g;
    const unsigned short *trv_lut_b;

    SHDISP_TRACE("in\n");

    if (clmr_trv_info.strength == SHDISP_LCDC_TRV_STRENGTH_00) {
        trv_lut_r = trv_lut_00_R;
        trv_lut_g = trv_lut_00_G;
        trv_lut_b = trv_lut_00_B;
    } else if (clmr_trv_info.strength == SHDISP_LCDC_TRV_STRENGTH_01) {
        trv_lut_r = trv_lut_01_R[clmr_trv_info.adjust];
        trv_lut_g = trv_lut_01_G[clmr_trv_info.adjust];
        trv_lut_b = trv_lut_01_B[clmr_trv_info.adjust];
    } else {
        trv_lut_r = trv_lut_02_R[clmr_trv_info.adjust];
        trv_lut_g = trv_lut_02_G[clmr_trv_info.adjust];
        trv_lut_b = trv_lut_02_B[clmr_trv_info.adjust];
    }

    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_TRV_LUT_WRITE, SHDISP_CLMR_FWCMD_HOST_TRV_LUT_WRITE_SIZE,
                                                               (unsigned char *)trv_lut_r);
    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_TRV_LUT_WRITE, SHDISP_CLMR_FWCMD_HOST_TRV_LUT_WRITE_SIZE,
                                                               (unsigned char *)trv_lut_g);
    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_TRV_LUT_WRITE, SHDISP_CLMR_FWCMD_HOST_TRV_LUT_WRITE_SIZE,
                                                               (unsigned char *)trv_lut_b);
    ret = shdisp_FWCMD_buf_finish();
    if (ret == SHDISP_RESULT_SUCCESS) {
        ret = shdisp_FWCMD_doKick(1, 0, NULL);
    }
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("TRV LUT write(%d) Error!!!\n", i);
        return ret;
    }

    ret = shdisp_clmr_lut_on_(SHDISP_CLMR_FWCMD_LUT_ON_TRV, 0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_lut_on_ Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_trv_lut_write                                                 */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_trv_lut_write(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_trv_lut_write_();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_trv_lut_write_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("TRV LUT write Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_trv_param_set_                                                */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_trv_param_set_(void)
{
    int ret = SHDISP_RESULT_SUCCESS;
    int size = ARRAY_SIZE(custom_trv_set_parm);
#ifdef CONFIG_TOUCHSCREEN_SHTPS
    int ptn;
#endif /* CONFIG_TOUCHSCREEN_SHTPS */
    int freq_type;

    SHDISP_TRACE("in\n");

#ifdef CONFIG_TOUCHSCREEN_SHTPS
    ptn = msm_tps_get_veilview_pattern();
    if (ptn == SHTPS_VEILVIEW_PATTERN_RGB_CHIDORI_1H) {
        custom_trv_set_parm[3].data = 0x05550AAA;
        custom_trv_set_parm[4].data = 0x05550AAA;
    } else if (ptn == SHTPS_VEILVIEW_PATTERN_RGB_CHIDORI_2H) {
        custom_trv_set_parm[3].data = 0x05550555;
        custom_trv_set_parm[4].data = 0x0AAA0AAA;
    } else if (ptn == SHTPS_VEILVIEW_PATTERN_MONOCHROME_1H) {
        custom_trv_set_parm[3].data = 0x01C70E38;
        custom_trv_set_parm[4].data = 0x01C70E38;
    } else if (ptn == SHTPS_VEILVIEW_PATTERN_MONOCHROME_2H) {
        custom_trv_set_parm[3].data = 0x0E380E38;
        custom_trv_set_parm[4].data = 0x01C701C7;
    } else {
        SHDISP_ERR("msm_tps_get_veilview_pattern Error!!!\n");
    }
    SHDISP_DEBUG("TRVPAT0(%08lx) TRVPAT1(%08lx).\n", custom_trv_set_parm[3].data, custom_trv_set_parm[4].data);
#endif /* CONFIG_TOUCHSCREEN_SHTPS */
    freq_type = shdisp_panel_API_get_drive_freq();
    switch (freq_type) {
    case SHDISP_MAIN_DISP_DRIVE_FREQ_TYPE_A:
        custom_trv_set_parm[10].data = (CALI_PTGHP_VAL + CALI_PTGHB_VAL + CALI_PTGHF_VAL + 3) << 16;
        break;
    case SHDISP_MAIN_DISP_DRIVE_FREQ_TYPE_B:
        custom_trv_set_parm[10].data = (CALI_PTGHP_VAL_B + CALI_PTGHB_VAL_B + CALI_PTGHF_VAL_B + 3) << 16;
        break;
    case SHDISP_MAIN_DISP_DRIVE_FREQ_TYPE_C:
        custom_trv_set_parm[10].data = (CALI_PTGHP_VAL_C + CALI_PTGHB_VAL_C + CALI_PTGHF_VAL_C + 3) << 16;
        break;
    default:
        break;
    }
    SHDISP_DEBUG("TRVCSCREF = %08lx.\n", custom_trv_set_parm[10].data);
    SHDISP_DEBUG("TRVAWHS0  = %08lx.\n", custom_trv_set_parm[8].data);
    SHDISP_DEBUG("TRVCFG    = %08lx.\n", custom_trv_set_parm[11].data);
    ret = shdisp_clmr_regsSetbyFW(custom_trv_set_parm, size);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_regsSetbyFW(custom_trv_set_parm) Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_trv_param_set                                                 */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_trv_param_set(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_trv_param_set_();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_trv_param_set_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("TRV Set Param Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_trv_on_                                                       */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_trv_on_(void)
{
    unsigned char wdata[SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE_SIZE];

    SHDISP_TRACE("in\n");

    wdata[0] =  SHDISP_CLMR_CUST_VSPCTRL2 & 0x00FF;
    wdata[1] = (SHDISP_CLMR_CUST_VSPCTRL2 & 0xFF00) >> 8;
    wdata[2] =  0x00000080 & 0x000000FF;
    wdata[3] = (0x00000080 & 0x0000FF00) >> 8;
    wdata[4] = (0x00000080 & 0x00FF0000) >> 16;
    wdata[5] = (0x00000080 & 0xFF000000) >> 24;
    wdata[6] =  0x00000080 & 0x000000FF;
    wdata[7] = (0x00000080 & 0x0000FF00) >> 8;
    wdata[8] = (0x00000080 & 0x00FF0000) >> 16;
    wdata[9] = (0x00000080 & 0xFF000000) >> 24;

    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE, SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE_SIZE, wdata);

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_trv_on                                                        */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_trv_on(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_trv_on_();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_trv_on_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("TRV On Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_trv_off                                                       */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_trv_off(void)
{
    SHDISP_TRACE("in\n");
    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_trv_filter_off_                                               */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_trv_filter_off_(void)
{
    int ret = SHDISP_RESULT_SUCCESS;
    int size = ARRAY_SIZE(custom_trv_filter_off);

    SHDISP_TRACE("in\n");

    ret = shdisp_clmr_regsSetbyFW(custom_trv_filter_off, size);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_regsSetbyFW(custom_trv_filter_off) Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_trv_filter_off                                                */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_trv_filter_off(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_trv_filter_off_();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_trv_filter_off_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("TRV Filter Off Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_trv_filter_on_                                                */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_trv_filter_on_(void)
{
    int ret = SHDISP_RESULT_SUCCESS;
    int size = ARRAY_SIZE(custom_trv_filter_on);

    SHDISP_TRACE("in\n");

    ret = shdisp_clmr_regsSetbyFW(custom_trv_filter_on, size);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_regsSetbyFW(custom_trv_filter_on) Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_trv_filter_on                                                 */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_trv_filter_on(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_trv_filter_on_();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_trv_filter_on_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("TRV Filter On Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sbl_on_setting_                                               */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sbl_on_setting_(unsigned char mode, unsigned short ap_type)
{
    int ret = SHDISP_RESULT_SUCCESS;
    int size = ARRAY_SIZE(custom_sbl_on_setting_common);

    SHDISP_TRACE("in mode = %d ap_type = %d\n", mode, ap_type);

    ret = shdisp_clmr_regsSetbyFW(custom_sbl_on_setting_common, size);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_regsSetbyFW(custom_sbl_on_setting_common) Error!!!\n");
        return ret;
    }

    size = ARRAY_SIZE(custom_sbl_on_setting[0][ap_type]);
    ret = shdisp_clmr_regsSetbyFW(custom_sbl_on_setting[0][ap_type], size);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_regsSetbyFW(custom_sbl_on_setting_common) Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sbl_on_setting                                                */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sbl_on_setting(unsigned char mode, unsigned short ap_type)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_sbl_on_setting_(mode, ap_type);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_sbl_on_setting_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("SBL On Setting Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sbl_set_luxmode                                               */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sbl_set_luxmode(unsigned char luxmode)
{

    SHDISP_TRACE("in\n");

    shdisp_bdic_API_set_lux_mode_modify(luxmode, SHDISP_CLMR_FWCMD_LIGHTCTL_LUXMODE_SET_SBL);

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sbl_on                                                        */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sbl_on(void)
{
    SHDISP_TRACE("in\n");
    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sbl_off                                                       */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_sbl_off(void)
{
    SHDISP_TRACE("in\n");
    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_svct_on_                                                      */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_svct_on_(unsigned short mode, unsigned short ap_type)
{
    int ret = SHDISP_RESULT_SUCCESS;
    int count = 0;
    int size = 0;

    SHDISP_TRACE("in mode = %d ap_type = %d\n", mode, ap_type);

    size = ARRAY_SIZE(custom_svct_setting[0][0]);
    ret = shdisp_clmr_regsSetbyFW(custom_svct_setting[mode - 1][ap_type], size);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_regsSetbyFW(custom_svct_setting) Error!!!\n");
        return ret;
    }

    size = ARRAY_SIZE(custom_svct_on);
    for (count = 0; count < size; count++) {
        custom_svct_on[count].data = svct_on_param[mode - 1][ap_type][count];
        SHDISP_DEBUG("custom_svct_on[%d].data = %08lx.\n", count, custom_svct_on[count].data);
    }
    ret = shdisp_clmr_regsSetbyFW(custom_svct_on, size);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_regsSetbyFW(custom_svct_on) Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_svct_on                                                       */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_svct_on(unsigned short mode, unsigned short ap_type)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in mode = %d ap_type = %d\n", mode, ap_type);

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_svct_on_(mode, ap_type);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_svct_on_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("SVCT ON Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_svct_off_                                                     */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_svct_off_(void)
{
    int ret = SHDISP_RESULT_SUCCESS;
    int size = ARRAY_SIZE(custom_svct_off);

    SHDISP_TRACE("in\n");

    ret = shdisp_clmr_regsSetbyFW(custom_svct_off, size);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_regsSetbyFW(custom_svct_off) Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_svct_off                                                      */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_svct_off(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_svct_off_();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_svct_off_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("SVCT OFF Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_hsv_param_set_                                                */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_hsv_param_set_(unsigned short mode, unsigned short ap_type)
{
    int ret = SHDISP_RESULT_SUCCESS;
    int size = ARRAY_SIZE(custom_hsv_config[0][0]);

    SHDISP_TRACE("in mode = %d ap_type = %d\n", mode, ap_type);

    ret = shdisp_clmr_regsSetbyFW(custom_hsv_config[mode - 1][ap_type], size);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_regsSetbyFW(custom_hsv_config) Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_hsv_param_set                                                 */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_hsv_param_set(unsigned short mode, unsigned short ap_type)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in mode = %d ap_type = %d\n", mode, ap_type);

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_hsv_param_set_(mode, ap_type);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_hsv_param_set_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("HSV Param Set Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_hsv_on_                                                       */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_hsv_on_(unsigned short mode, unsigned short ap_type)
{
    unsigned char wdata[SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE_SIZE];

    SHDISP_TRACE("in mode = %d ap_type = %d\n", mode, ap_type);

    wdata[0] =  SHDISP_CLMR_CUST_VSPCTRL1 & 0x00FF;
    wdata[1] = (SHDISP_CLMR_CUST_VSPCTRL1 & 0xFF00) >> 8;
    wdata[2] =  0x00040000 & 0x000000FF;
    wdata[3] = (0x00040000 & 0x0000FF00) >> 8;
    wdata[4] = (0x00040000 & 0x00FF0000) >> 16;
    wdata[5] = (0x00040000 & 0xFF000000) >> 24;
    wdata[6] =  hsv_on_param[mode - 1][ap_type][0] & 0x000000FF;
    wdata[7] = (hsv_on_param[mode - 1][ap_type][0] & 0x0000FF00) >> 8;
    wdata[8] = (hsv_on_param[mode - 1][ap_type][0] & 0x00FF0000) >> 16;
    wdata[9] = (hsv_on_param[mode - 1][ap_type][0] & 0xFF000000) >> 24;

    SHDISP_DEBUG("wdata = %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
                    wdata[0], wdata[1], wdata[2], wdata[3], wdata[4],
                    wdata[5], wdata[6], wdata[7], wdata[8], wdata[9]);
    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE, SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE_SIZE, wdata);

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_hsv_on                                                        */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_hsv_on(unsigned short mode, unsigned short ap_type)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in mode = %d ap_type = %d\n", mode, ap_type);

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_hsv_on_(mode, ap_type);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_hsv_on_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("HSV ON Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_hsv_off_                                                      */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_hsv_off_(void)
{
    unsigned char wdata[SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE_SIZE];

    SHDISP_TRACE("in\n");

    wdata[0] =  SHDISP_CLMR_CUST_VSPCTRL1 & 0x00FF;
    wdata[1] = (SHDISP_CLMR_CUST_VSPCTRL1 & 0xFF00) >> 8;
    wdata[2] =  0x00040000 & 0x000000FF;
    wdata[3] = (0x00040000 & 0x0000FF00) >> 8;
    wdata[4] = (0x00040000 & 0x00FF0000) >> 16;
    wdata[5] = (0x00040000 & 0xFF000000) >> 24;
    wdata[6] =  0x00000000 & 0x000000FF;
    wdata[7] = (0x00000000 & 0x0000FF00) >> 8;
    wdata[8] = (0x00000000 & 0x00FF0000) >> 16;
    wdata[9] = (0x00000000 & 0xFF000000) >> 24;

    SHDISP_DEBUG("wdata = %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
                    wdata[0], wdata[1], wdata[2], wdata[3], wdata[4],
                    wdata[5], wdata[6], wdata[7], wdata[8], wdata[9]);
    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE, SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE_SIZE, wdata);

    wdata[0] =  SHDISP_CLMR_CUST_EACT0 & 0x00FF;
    wdata[1] = (SHDISP_CLMR_CUST_EACT0 & 0xFF00) >> 8;
    wdata[2] =  0x00000001 & 0x000000FF;
    wdata[3] = (0x00000001 & 0x0000FF00) >> 8;
    wdata[4] = (0x00000001 & 0x00FF0000) >> 16;
    wdata[5] = (0x00000001 & 0xFF000000) >> 24;
    wdata[6] =  0x00000000 & 0x000000FF;
    wdata[7] = (0x00000000 & 0x0000FF00) >> 8;
    wdata[8] = (0x00000000 & 0x00FF0000) >> 16;
    wdata[9] = (0x00000000 & 0xFF000000) >> 24;

    SHDISP_DEBUG("wdata = %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
                    wdata[0], wdata[1], wdata[2], wdata[3], wdata[4],
                    wdata[5], wdata[6], wdata[7], wdata[8], wdata[9]);
    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE, SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE_SIZE, wdata);

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_hsv_off                                                       */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_hsv_off(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_hsv_off_();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_hsv_off_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("HSV OFF Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_pca_config_                                                   */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_pca_config_(unsigned short mode, unsigned short ap_type)
{
    int ret = SHDISP_RESULT_SUCCESS;
    int size = ARRAY_SIZE(custom_pca_config[0][0]);

    SHDISP_TRACE("in mode = %d ap_type = %d\n", mode, ap_type);

    ret = shdisp_clmr_regsSetbyFW(custom_pca_config[mode -  1][ap_type], size);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_regsSetbyFW(custom_pca_config) Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_pca_config                                                    */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_pca_config(unsigned short mode, unsigned short ap_type)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in mode = %d ap_type = %d\n", mode, ap_type);

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_pca_config_(mode, ap_type);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_pca_config_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("PCA Config Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_pca_on_                                                       */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_pca_on_(unsigned short mode, unsigned short ap_type)
{
    unsigned char wdata[SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE_SIZE];

    SHDISP_TRACE("in mode = %d ap_type = %d\n", mode, ap_type);

    wdata[0] =  SHDISP_CLMR_CUST_VSPCTRL1 & 0x00FF;
    wdata[1] = (SHDISP_CLMR_CUST_VSPCTRL1 & 0xFF00) >> 8;
    wdata[2] =  0x00200000 & 0x000000FF;
    wdata[3] = (0x00200000 & 0x0000FF00) >> 8;
    wdata[4] = (0x00200000 & 0x00FF0000) >> 16;
    wdata[5] = (0x00200000 & 0xFF000000) >> 24;
    wdata[6] =  pca_on_param[mode - 1][ap_type][0] & 0x000000FF;
    wdata[7] = (pca_on_param[mode - 1][ap_type][0] & 0x0000FF00) >> 8;
    wdata[8] = (pca_on_param[mode - 1][ap_type][0] & 0x00FF0000) >> 16;
    wdata[9] = (pca_on_param[mode - 1][ap_type][0] & 0xFF000000) >> 24;

    SHDISP_DEBUG("wdata = %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
                    wdata[0], wdata[1], wdata[2], wdata[3], wdata[4],
                    wdata[5], wdata[6], wdata[7], wdata[8], wdata[9]);
    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE, SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE_SIZE, wdata);

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_pca_on                                                        */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_pca_on(unsigned short mode, unsigned short ap_type)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in mode = %d ap_type = %d\n", mode, ap_type);

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_pca_on_(mode, ap_type);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_pca_on_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("PCA ON Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_pca_off_                                                      */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_pca_off_(void)
{
    unsigned char wdata[SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE_SIZE];

    SHDISP_TRACE("in\n");

    wdata[0] =  SHDISP_CLMR_CUST_VSPCTRL1 & 0x00FF;
    wdata[1] = (SHDISP_CLMR_CUST_VSPCTRL1 & 0xFF00) >> 8;
    wdata[2] =  0x00200000 & 0x000000FF;
    wdata[3] = (0x00200000 & 0x0000FF00) >> 8;
    wdata[4] = (0x00200000 & 0x00FF0000) >> 16;
    wdata[5] = (0x00200000 & 0xFF000000) >> 24;
    wdata[6] =  0x00000000 & 0x000000FF;
    wdata[7] = (0x00000000 & 0x0000FF00) >> 8;
    wdata[8] = (0x00000000 & 0x00FF0000) >> 16;
    wdata[9] = (0x00000000 & 0xFF000000) >> 24;

    SHDISP_DEBUG("wdata = %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
                    wdata[0], wdata[1], wdata[2], wdata[3], wdata[4],
                    wdata[5], wdata[6], wdata[7], wdata[8], wdata[9]);
    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE, SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE_SIZE, wdata);

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_pca_off                                                       */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_pca_off(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_pca_off_();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_pca_off_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("PCA OFF Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_cpf_lut_write_                                                */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_cpf_lut_write_(unsigned short mode, unsigned short ap_type)
{
    int i = 0;
    const unsigned short *cpf_lut;

    SHDISP_TRACE("in mode = %d ap_type = %d\n", mode, ap_type);

    for (i = 0; i < 3; i++) {
        if (i == 0) {
            cpf_lut = cpf_lut1_R[mode - 1][ap_type];
        } else if (i == 1) {
            cpf_lut = cpf_lut1_G[mode - 1][ap_type];
        } else {
            cpf_lut = cpf_lut1_B[mode - 1][ap_type];
        }
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_CPF1_LUT_WRITE, SHDISP_CLMR_FWCMD_HOST_CPF1_LUT_WRITE_SIZE,
                                                                    (unsigned char *)cpf_lut);
    }

    for (i = 0; i < 3; i++) {
        if (i == 0) {
            cpf_lut = (unsigned short *)cpf_lut2_R[mode - 1][ap_type];
        } else if (i == 1) {
            cpf_lut = (unsigned short *)cpf_lut2_G[mode - 1][ap_type];
        } else {
            cpf_lut = (unsigned short *)cpf_lut2_B[mode - 1][ap_type];
        }
        cpf_lut++;
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_CPF2_LUT_WRITE, SHDISP_CLMR_FWCMD_HOST_CPF2_LUT_WRITE_SIZE,
                                                                    (unsigned char *)cpf_lut);
    }


    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_cpf_lut_write                                                 */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_cpf_lut_write(unsigned short mode, unsigned short ap_type)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in mode = %d ap_type = %d\n", mode, ap_type);

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_cpf_lut_write_(mode, ap_type);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_cpf_lut_write_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("CPF LUT write Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_cpf_param_set_                                                */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_cpf_param_set_(unsigned short mode, unsigned short ap_type)
{
    int ret = SHDISP_RESULT_SUCCESS;
    int size = ARRAY_SIZE(custom_cpf_parameta[0][0]);

    SHDISP_TRACE("in mode = %d ap_type = %d\n", mode, ap_type);

    ret = shdisp_clmr_regsSetbyFW(custom_cpf_parameta[mode - 1][ap_type], size);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_regsSetbyFW(custom_cpf_parameta) Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_cpf_param_set                                                 */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_cpf_param_set(unsigned short mode, unsigned short ap_type)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in mode = %d ap_type = %d\n", mode, ap_type);

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_cpf_param_set_(mode, ap_type);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_cpf_param_set_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("CPF Set Param Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_cpf_on_                                                       */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_cpf_on_(unsigned short mode)
{
    unsigned char wdata[SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE_SIZE];

    SHDISP_TRACE("in mode = %d\n", mode);

    wdata[0] =  SHDISP_CLMR_CUST_CPFCTRL0 & 0x00FF;
    wdata[1] = (SHDISP_CLMR_CUST_CPFCTRL0 & 0xFF00) >> 8;
    wdata[2] =  0x00001004 & 0x000000FF;
    wdata[3] = (0x00001004 & 0x0000FF00) >> 8;
    wdata[4] = (0x00001004 & 0x00FF0000) >> 16;
    wdata[5] = (0x00001004 & 0xFF000000) >> 24;
    wdata[6] =  cpf_mode_param[mode] & 0x000000FF;
    wdata[7] = (cpf_mode_param[mode] & 0x0000FF00) >> 8;
    wdata[8] = (cpf_mode_param[mode] & 0x00FF0000) >> 16;
    wdata[9] = (cpf_mode_param[mode] & 0xFF000000) >> 24;

    SHDISP_DEBUG("wdata = %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
                    wdata[0], wdata[1], wdata[2], wdata[3], wdata[4],
                    wdata[5], wdata[6], wdata[7], wdata[8], wdata[9]);
    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE, SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE_SIZE, wdata);

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_cpf_on                                                        */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_cpf_on(unsigned short mode)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in mode = %d\n", mode);

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_cpf_on_(mode);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_cpf_on_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("CPF ON Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_cpf_off_                                                      */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_cpf_off_(void)
{
    unsigned char wdata[SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE_SIZE];

    SHDISP_TRACE("in\n");

    wdata[0] =  SHDISP_CLMR_CUST_CPFCTRL0 & 0x00FF;
    wdata[1] = (SHDISP_CLMR_CUST_CPFCTRL0 & 0xFF00) >> 8;
    wdata[2] =  0x00001004 & 0x000000FF;
    wdata[3] = (0x00001004 & 0x0000FF00) >> 8;
    wdata[4] = (0x00001004 & 0x00FF0000) >> 16;
    wdata[5] = (0x00001004 & 0xFF000000) >> 24;
    wdata[6] =  0x00000000 & 0x000000FF;
    wdata[7] = (0x00000000 & 0x0000FF00) >> 8;
    wdata[8] = (0x00000000 & 0x00FF0000) >> 16;
    wdata[9] = (0x00000000 & 0xFF000000) >> 24;

    SHDISP_DEBUG("wdata = %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
                    wdata[0], wdata[1], wdata[2], wdata[3], wdata[4],
                    wdata[5], wdata[6], wdata[7], wdata[8], wdata[9]);
    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE, SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE_SIZE, wdata);

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_cpf_off                                                       */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_cpf_off(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_cpf_off_();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_cpf_off_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("CPF ON Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_cpf_lut_rewrite_                                              */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_cpf_lut_rewrite_(unsigned short mode, unsigned short ap_type)
{
    int ret = SHDISP_RESULT_SUCCESS;
    int i = 0;
    const unsigned short *cpf_lut;

    SHDISP_TRACE("in mode = %d ap_type = %d\n", mode, ap_type);

    for (i = 0; i < 3; i++) {
        if (i == 0) {
            cpf_lut = cpf_lut1_R[mode - 1][ap_type];
        } else if (i == 1) {
            cpf_lut = cpf_lut1_G[mode - 1][ap_type];
        } else {
            cpf_lut = cpf_lut1_B[mode - 1][ap_type];
        }
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_CPF1_LUT_WRITE, SHDISP_CLMR_FWCMD_HOST_CPF1_LUT_WRITE_SIZE,
                                                                    (unsigned char *)cpf_lut);
    }

    for (i = 0; i < 3; i++) {
        if (i == 0) {
            cpf_lut = (unsigned short *)cpf_lut2_R[mode - 1][ap_type];
        } else if (i == 1) {
            cpf_lut = (unsigned short *)cpf_lut2_G[mode - 1][ap_type];
        } else {
            cpf_lut = (unsigned short *)cpf_lut2_B[mode - 1][ap_type];
        }
        cpf_lut++;
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_CPF2_LUT_WRITE, SHDISP_CLMR_FWCMD_HOST_CPF2_LUT_WRITE_SIZE,
                                                                    (unsigned char *)cpf_lut);
    }

    ret = shdisp_clmr_lut_on_(SHDISP_CLMR_FWCMD_LUT_ON_CPF, 0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_lut_on_ Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_cpf_lut_rewrite                                               */
/* ------------------------------------------------------------------------- */
int shdisp_clmr_cpf_lut_rewrite(unsigned short mode, unsigned short ap_type)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in mode = %d ap_type = %d\n", mode, ap_type);

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_cpf_lut_rewrite_(mode, ap_type);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_cpf_lut_rewrite_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("CPF LUT write Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_smite_on_                                                     */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_smite_on_(unsigned short mode, unsigned short ap_type)
{
    unsigned char wdata[SHDISP_CLMR_FWCMD_SMITE_SET_MODE_SIZE];

    SHDISP_TRACE("in mode = %d ap_type = %d\n", mode, ap_type);

    wdata[0] = mode & 0xFF;
    wdata[1] = (mode >> 8) & 0xFF;

    if (mode == SHDISP_CLMR_FWCMD_SMITE_SET_MODE_LPMC) {
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_SMITE_WRITE_CONFIG, SHDISP_CLMR_FWCMD_SMITE_WRITE_CONFIG_SIZE_32,
                             (unsigned char *)smite_config_lpmc[smite_config_lpmc_mode[ap_type]]);
    } else {
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_SMITE_WRITE_CONFIG, SHDISP_CLMR_FWCMD_SMITE_WRITE_CONFIG_SIZE_128,
                                                                   (unsigned char *)smite_config_common);
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_SMITE_WRITE_CONFIG, SHDISP_CLMR_FWCMD_SMITE_WRITE_CONFIG_SIZE_32,
                                                                   (unsigned char *)smite_config_dbc);
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_SMITE_WRITE_CONFIG, SHDISP_CLMR_FWCMD_SMITE_WRITE_CONFIG_SIZE_32,
                                                                   (unsigned char *)smite_config_acc0);
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_SMITE_WRITE_CONFIG, SHDISP_CLMR_FWCMD_SMITE_WRITE_CONFIG_SIZE_32,
                                                                   (unsigned char *)smite_config_acc1);
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_SMITE_WRITE_CONFIG, SHDISP_CLMR_FWCMD_SMITE_WRITE_CONFIG_SIZE_32,
                             (unsigned char *)smite_config_lpmc[smite_config_lpmc_mode[ap_type]]);
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_SMITE_WRITE_CONFIG, SHDISP_CLMR_FWCMD_SMITE_WRITE_CONFIG_SIZE_64,
                                                                   (unsigned char *)smite_config_blr);
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_SMITE_WRITE_CONFIG, SHDISP_CLMR_FWCMD_SMITE_WRITE_CONFIG_SIZE_64,
                                                                   (unsigned char *)smite_config_pwm);
    }
    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_SMITE_SET_MODE,     SHDISP_CLMR_FWCMD_SMITE_SET_MODE_SIZE,         wdata);
    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_SMITE_COMMIT,       SHDISP_CLMR_FWCMD_SMITE_COMMIT_SIZE,           NULL);

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_smite_on                                                      */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_smite_on(unsigned short mode, unsigned short ap_type)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in mode = %d ap_type = %d\n", mode, ap_type);

    if ((mode != SHDISP_CLMR_FWCMD_SMITE_SET_MODE_LPMC)
     && (mode != SHDISP_CLMR_FWCMD_SMITE_SET_MODE_DBC_ACC)
     && (mode != SHDISP_CLMR_FWCMD_SMITE_SET_MODE_DBC)
     && (mode != SHDISP_CLMR_FWCMD_SMITE_SET_MODE_ACC)) {
        SHDISP_ERR("<INVALID_VALUE> mode(%d) auto_mode(%d).\n", clmr_dbc.mode, clmr_dbc.auto_mode);
        return SHDISP_RESULT_FAILURE;
    }

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_smite_on_(mode, ap_type);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_smite_on_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("SMITE On Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_smite_mode_chg_                                               */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_smite_mode_chg_(unsigned short mode)
{
    unsigned char wdata[SHDISP_CLMR_FWCMD_SMITE_SET_MODE_SIZE];

    SHDISP_TRACE("in mode = %d\n", mode);

#if defined(CONFIG_SHDISP_PANEL_ANDY) || defined(CONFIG_SHDISP_PANEL_RYOMA) || defined(CONFIG_SHDISP_PANEL_GEMINI)
    if ((sh_boot_get_bootmode() == SH_BOOT_D || sh_boot_get_bootmode() == SH_BOOT_F_F)
     && (mode == SHDISP_CLMR_FWCMD_SMITE_SET_MODE_LPMC)) {
            mode = SHDISP_CLMR_FWCMD_SMITE_SET_MODE_OFF;
    }
#endif

    wdata[0] =  mode & 0xFF;
    wdata[1] = (mode >> 8) & 0xFF;

    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_SMITE_SET_MODE, SHDISP_CLMR_FWCMD_SMITE_SET_MODE_SIZE, wdata);
    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_SMITE_COMMIT,   SHDISP_CLMR_FWCMD_SMITE_COMMIT_SIZE,   NULL);

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_smite_mode_chg                                                */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_smite_mode_chg(unsigned short mode)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in mode = %d\n", mode);

    if ((mode != SHDISP_CLMR_FWCMD_SMITE_SET_MODE_LPMC)
     && (mode != SHDISP_CLMR_FWCMD_SMITE_SET_MODE_DBC_ACC)
     && (mode != SHDISP_CLMR_FWCMD_SMITE_SET_MODE_DBC)
     && (mode != SHDISP_CLMR_FWCMD_SMITE_SET_MODE_ACC)) {
        SHDISP_ERR("<INVALID_VALUE> mode(%d) auto_mode(%d).\n", clmr_dbc.mode, clmr_dbc.auto_mode);
        return SHDISP_RESULT_FAILURE;
    }

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_smite_mode_chg_(mode);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_smite_mode_chg_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("SMITE Mode Chg Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_smite_off_                                                    */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_smite_off_(void)
{
    unsigned char wdata[SHDISP_CLMR_FWCMD_SMITE_SET_MODE_SIZE];
    unsigned short mode;

    SHDISP_TRACE("in\n");

#if defined(CONFIG_SHDISP_PANEL_ANDY) || defined(CONFIG_SHDISP_PANEL_RYOMA) || defined(CONFIG_SHDISP_PANEL_GEMINI)
    if (sh_boot_get_bootmode() == SH_BOOT_D || sh_boot_get_bootmode() == SH_BOOT_F_F) {
        mode = SHDISP_CLMR_FWCMD_SMITE_SET_MODE_OFF;
    } else {
        mode = SHDISP_CLMR_FWCMD_SMITE_SET_MODE_LPMC;
    }
#else
    mode = SHDISP_CLMR_FWCMD_SMITE_SET_MODE_OFF;
#endif

    wdata[0] =  mode & 0xFF;
    wdata[1] = (mode >> 8) & 0xFF;

    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_SMITE_SET_MODE, SHDISP_CLMR_FWCMD_SMITE_SET_MODE_SIZE, wdata);
    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_SMITE_COMMIT,   SHDISP_CLMR_FWCMD_SMITE_COMMIT_SIZE,   NULL);

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_smite_off                                                     */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_smite_off(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_smite_off_();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_smite_off_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("SMITE Off Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_smite_lpmc_setting_chg_                                       */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_smite_lpmc_setting_chg_(unsigned short ap_type)
{
    unsigned char wdata[SHDISP_CLMR_FWCMD_SMITE_SET_MODE_SIZE];
    const unsigned char *set;

    SHDISP_TRACE("in ap_type = %d\n", ap_type);

    set = shdisp_clmr_get_pic_adj_matrix(clmr_trv_info.status, &clmr_dbc, &clmr_pic_adj, ap_type);
    wdata[0] =  smite_matrix[set[PIC_ADJ_MATRIX_SMITE]] & 0xF7;
    wdata[1] = (smite_matrix[set[PIC_ADJ_MATRIX_SMITE]] >> 8) & 0xFF;
    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_SMITE_SET_MODE,     SHDISP_CLMR_FWCMD_SMITE_SET_MODE_SIZE,         wdata);
    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_SMITE_COMMIT,       SHDISP_CLMR_FWCMD_SMITE_COMMIT_SIZE,           NULL);
    shdisp_SYS_cmd_delay_us(WAIT_1FRAME_US);

    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_SMITE_WRITE_CONFIG, SHDISP_CLMR_FWCMD_SMITE_WRITE_CONFIG_SIZE_32,
                         (unsigned char *)smite_config_lpmc[smite_config_lpmc_mode[ap_type]]);
    wdata[0] =  smite_matrix[set[PIC_ADJ_MATRIX_SMITE]] & 0xFF;
    wdata[1] = (smite_matrix[set[PIC_ADJ_MATRIX_SMITE]] >> 8) & 0xFF;
    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_SMITE_SET_MODE,     SHDISP_CLMR_FWCMD_SMITE_SET_MODE_SIZE,         wdata);
    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_SMITE_COMMIT,       SHDISP_CLMR_FWCMD_SMITE_COMMIT_SIZE,           NULL);

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_smite_lpmc_setting_chg                                        */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_smite_lpmc_setting_chg(unsigned short ap_type)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in ap_type = %d\n", ap_type);

    if (ap_type >= NUM_SHDISP_LCDC_PIC_ADJ_AP) {
        SHDISP_ERR("<INVALID_VALUE> ap_type = %d.\n", ap_type);
        return SHDISP_RESULT_FAILURE;
    }

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_smite_lpmc_setting_chg_(ap_type);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_smite_lpmc_setting_chg_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("SMITE LPMC CHG Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_ae_param_set_                                                 */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_ae_param_set_(void)
{
    int i;

    SHDISP_TRACE("in\n");

    for (i = 0; i < SHDISP_CLMR_AE_PARAM_SIZE; i++) {
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_LIGHTCTL_WRITE, SHDISP_CLMR_FWCMD_LIGHTCTL_WRITE_SIZE_FOR_AE,
                                                               (unsigned char *)ae_param[i]);
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_ae_param_set                                                  */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_ae_param_set(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_ae_param_set_();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_ae_param_set_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("AE Param Set Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_ae_time_set_                                                  */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_ae_time_set_(unsigned char time)
{
    unsigned char wdata[SHDISP_CLMR_FWCMD_AE_TIME_SET_SIZE];

    SHDISP_TRACE("in time = %d\n", time);

    wdata[0] = time;

    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_AE_TIME_SET, SHDISP_CLMR_FWCMD_AE_TIME_SET_SIZE, wdata);

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_ae_time_set                                                   */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_ae_time_set(unsigned char time)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in time = %d\n", time);

    if (time > SHDISP_MAIN_DISP_AE_TIME_MORNING) {
        SHDISP_ERR("<INVALID_VALUE> time(%d).\n", time);
        return SHDISP_RESULT_FAILURE;
    }

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_ae_time_set_(time);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_ae_time_set_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("AE Param Set Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_ae_on_                                                        */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_ae_on_(void)
{
    unsigned char wdata[SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE_SIZE];

    SHDISP_TRACE("in\n");

    wdata[0] =  SHDISP_CLMR_CUST_VSPCTRL1 & 0x00FF;
    wdata[1] = (SHDISP_CLMR_CUST_VSPCTRL1 & 0xFF00) >> 8;
    wdata[2] =  0x00240000 & 0x000000FF;
    wdata[3] = (0x00240000 & 0x0000FF00) >> 8;
    wdata[4] = (0x00240000 & 0x00FF0000) >> 16;
    wdata[5] = (0x00240000 & 0xFF000000) >> 24;
    wdata[6] =  0x00240000 & 0x000000FF;
    wdata[7] = (0x00240000 & 0x0000FF00) >> 8;
    wdata[8] = (0x00240000 & 0x00FF0000) >> 16;
    wdata[9] = (0x00240000 & 0xFF000000) >> 24;

    SHDISP_DEBUG("wdata = %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
                    wdata[0], wdata[1], wdata[2], wdata[3], wdata[4],
                    wdata[5], wdata[6], wdata[7], wdata[8], wdata[9]);
    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE, SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE_SIZE, wdata);

    wdata[0] = SHDISP_CLMR_AE_ON;

    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_AE_MODE, SHDISP_CLMR_FWCMD_AE_MODE_SIZE, wdata);

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_ae_on                                                         */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_ae_on(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_ae_on_();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_ae_on_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("AE ON Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_ae_off_                                                       */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_ae_off_(unsigned char mode)
{
    unsigned char wdata[SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE_SIZE];

    SHDISP_TRACE("in mode = %d\n", mode);

    wdata[0] = SHDISP_CLMR_AE_OFF;

    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_AE_MODE, SHDISP_CLMR_FWCMD_AE_MODE_SIZE, wdata);

    if (mode == SHDISP_CLMR_AE_OFF_WITH_HSV_PCA) {
        wdata[0] =  SHDISP_CLMR_CUST_VSPCTRL1 & 0x00FF;
        wdata[1] = (SHDISP_CLMR_CUST_VSPCTRL1 & 0xFF00) >> 8;
        wdata[2] =  0x00240000 & 0x000000FF;
        wdata[3] = (0x00240000 & 0x0000FF00) >> 8;
        wdata[4] = (0x00240000 & 0x00FF0000) >> 16;
        wdata[5] = (0x00240000 & 0xFF000000) >> 24;
        wdata[6] =  0x00000000 & 0x000000FF;
        wdata[7] = (0x00000000 & 0x0000FF00) >> 8;
        wdata[8] = (0x00000000 & 0x00FF0000) >> 16;
        wdata[9] = (0x00000000 & 0xFF000000) >> 24;

        SHDISP_DEBUG("wdata = %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
                        wdata[0], wdata[1], wdata[2], wdata[3], wdata[4],
                        wdata[5], wdata[6], wdata[7], wdata[8], wdata[9]);
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE, SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE_SIZE,
                                                                                                              wdata);
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_ae_off                                                        */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_ae_off(unsigned char mode)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in mode = %d\n", mode);

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);
    ret = shdisp_clmr_ae_off_(mode);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_ae_off_ Error!!!\n");
    } else {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    shdisp_FWCMD_buf_set_nokick(0);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("AE OFF Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_api_set_flicker_trv                                           */
/* ------------------------------------------------------------------------- */
int shdisp_clmr_api_set_flicker_trv(struct shdisp_flicker_trv *flicker_trv)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    if (flicker_trv->request == SHDISP_LCDC_FLICKER_TRV_ON) {
        ret = shdisp_clmr_flicker_trv_on(flicker_trv->level, flicker_trv->type);
    } else if (flicker_trv->request == SHDISP_LCDC_FLICKER_TRV_OFF) {
        ret = shdisp_clmr_flicker_trv_off();
    } else {
        SHDISP_ERR("<INVALID_VALUE> flicker_trv->request (%d).\n", flicker_trv->request);
        ret = SHDISP_RESULT_FAILURE;
    }
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_flicker_trv_on/off(%d) error. ret = %d\n", flicker_trv->request, ret);
        ret = -1;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_flicker_trv_on                                                */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_flicker_trv_on(unsigned char level, unsigned char type)
{
    int ret = SHDISP_RESULT_SUCCESS;
    int size = 0;
    int i, count = 0;

    SHDISP_TRACE("in\n");

    if (!(type == SHDISP_LCDC_FLICKER_TRV_COLUMN)
     && !(type == SHDISP_LCDC_FLICKER_TRV_DOT1H)
     && !(type == SHDISP_LCDC_FLICKER_TRV_DOT2H)) {
        SHDISP_ERR("<INVALID_VALUE> type (%d).\n", type);
        return SHDISP_RESULT_FAILURE;
    }

    ret = shdisp_clmr_flicker_trv_custom_set();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_flicker_trv_custom_set Error!!!\n");
        return ret;
    }

    size = ARRAY_SIZE(custom_flicker_trv_set_lut_adr);
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_OTHER);
    for (count = 0; count < size; count++) {
        ret = shdisp_clmr_regSetwithFW(&custom_flicker_trv_set_lut_adr[count]);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("LUT Adr Set Error!!!\n");
            return ret;
        }
    }

    custom_flicker_trv_set_lut_data[0].data = level | (level << 8);
    SHDISP_DEBUG("custom_flicker_trv_set_lut_data[0].data = %08lx.\n", custom_flicker_trv_set_lut_data[0].data);
    size = ARRAY_SIZE(custom_flicker_trv_set_lut_data);
    for (count = 0; count < size; count++) {
        for (i = 0; i < 256; i++) {
            ret = shdisp_clmr_regSetwithFW(&custom_flicker_trv_set_lut_data[count]);
            if (ret != SHDISP_RESULT_SUCCESS) {
                SHDISP_ERR("LUT Data Set Error!!!\n");
                return ret;
            }
        }
    }

    size = ARRAY_SIZE(custom_flicker_trv_set_lut3);
    for (count = 0; count < size; count++) {
        ret = shdisp_clmr_regSetwithFW(&custom_flicker_trv_set_lut3[count]);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("LUT Set (VSPCTRL2) Error!!!\n");
            return ret;
        }
    }

    if (type == SHDISP_LCDC_FLICKER_TRV_DOT1H) {
        custom_flicker_trv_set_trv[4].data = 0x05550AAA;
        custom_flicker_trv_set_trv[5].data = 0x05550AAA;
    } else if (type == SHDISP_LCDC_FLICKER_TRV_DOT2H) {
        custom_flicker_trv_set_trv[4].data = 0x05550555;
        custom_flicker_trv_set_trv[5].data = 0x0AAA0AAA;
    } else {
        custom_flicker_trv_set_trv[4].data = 0x05550555;
        custom_flicker_trv_set_trv[5].data = 0x05550555;
    }
    SHDISP_DEBUG("type = %d TRVPAT0(%08lx) TRVPAT1(%08lx).\n", type, custom_flicker_trv_set_trv[4].data,
                                                                     custom_flicker_trv_set_trv[5].data);
    size = ARRAY_SIZE(custom_flicker_trv_set_trv);
    ret = shdisp_clmr_regSet_multi(custom_flicker_trv_set_trv, size);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("TRV Set Error!!!\n");
        return ret;
    }

    size = ARRAY_SIZE(custom_flicker_trv_on);
    for (count = 0; count < size; count++) {
        ret = shdisp_clmr_regSetwithFW(&custom_flicker_trv_on[count]);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("TRV ON Error!!!\n");
            return ret;
        }
    }
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);

    ret = shdisp_clmr_flicker_trv_vsp_on_off(SHDISP_LCDC_FLICKER_TRV_ON);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_flicker_trv_vsp_on_off Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");

    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_flicker_trv_off                                               */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_flicker_trv_off(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in\n");

    ret = shdisp_clmr_flicker_trv_vsp_on_off(SHDISP_LCDC_FLICKER_TRV_OFF);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_flicker_trv_vsp_on_off() .\n");
        return ret;
    }

    SHDISP_TRACE("out\n");

    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_flicker_trv_vsp_on_off                                        */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_flicker_trv_vsp_on_off(int on)
{
    int ret = SHDISP_RESULT_SUCCESS;
    int size = 0;
    int count = 0;

    SHDISP_TRACE("in on=%d.\n", on);

    if (on > 0) {
        on = SHDISP_LCDC_FLICKER_TRV_ON;
    }

    custom_flicker_trv_vsp_on[0].data  = 1 << (19 + on);

    size = ARRAY_SIZE(custom_flicker_trv_vsp_on);
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_OTHER);
    for (count = 0; count < size; count++) {
        ret = shdisp_clmr_regSetwithFW(&custom_flicker_trv_vsp_on[count]);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("ON/OFF (%d) Error!!!\n", on);
            return ret;
        }
    }
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);

    ret = shdisp_clmr_vsp_on();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_vsp_on Error!!!\n");
        return ret;
    }

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_flicker_trv_custom_set                                        */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_flicker_trv_custom_set(void)
{
    int ret = SHDISP_RESULT_SUCCESS;
    int size = 0;
    int count = 0;

    SHDISP_TRACE("in\n");

    size = ARRAY_SIZE(custom_flicker_custom_set1);
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_OTHER);
    for (count = 0; count < size; count++) {
        ret = shdisp_clmr_regSetwithFW(&custom_flicker_custom_set1[count]);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("set1 Error!!!\n");
            return ret;
        }
    }

    shdisp_SYS_cmd_delay_us(1000);

    size = ARRAY_SIZE(custom_flicker_custom_set2);
    ret = shdisp_clmr_regSet_multi(custom_flicker_custom_set2, size);
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("set2 Error!!!\n");
        return ret;
    }

    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);

    ret = shdisp_clmr_vsp_on();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_vsp_on Error!!!\n");
        return ret;
    }

    size = ARRAY_SIZE(custom_flicker_custom_set3);
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_OTHER);
    for (count = 0; count < size; count++) {
        ret = shdisp_clmr_regSetwithFW(&custom_flicker_custom_set3[count]);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("set3 Error!!!\n");
            return ret;
        }
    }
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);

    ret = shdisp_clmr_vsp_on();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("shdisp_clmr_vsp_on Error!!!\n");
        return ret;
    }

    size = ARRAY_SIZE(custom_flicker_custom_set4);
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_OTHER);
    for (count = 0; count < size; count++) {
        ret = shdisp_clmr_regSetwithFW(&custom_flicker_custom_set4[count]);
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("set4 Error!!!\n");
            return ret;
        }
    }

    shdisp_SYS_cmd_delay_us(20 * 1000);
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_hsclk_on                                             */
/*---------------------------------------------------------------------------*/
void shdisp_clmr_api_hsclk_on(void)
{
    int count = 0;
    int size = ARRAY_SIZE(hsclk_on);

    SHDISP_TRACE("in\n");

        shdisp_clmr_regSet_multi(&hsclk_on[count], size);
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_hsclk_off                                            */
/*---------------------------------------------------------------------------*/
void shdisp_clmr_api_hsclk_off(void)
{
    int count = 0;
    int size = ARRAY_SIZE(hsclk_off);

    SHDISP_TRACE("in\n");

        shdisp_clmr_regSet_multi(&hsclk_off[count], size);
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_auto_pat_ctrl                                        */
/*---------------------------------------------------------------------------*/
void shdisp_clmr_api_auto_pat_ctrl(int sw)
{
    int size = 0;

    if (shdisp_pm_is_clmr_on() != SHDISP_DEV_STATE_ON) {
        SHDISP_ERR("<RESULT_FAILURE> clmr is not active. out\n");
        return;
    }
    SHDISP_TRACE("in\n");
    if (sw) {
        size = ARRAY_SIZE(auto_pat_on);
        shdisp_clmr_regSet_multi(auto_pat_on, size);
    } else {
        size = ARRAY_SIZE(auto_pat_off);
        shdisp_clmr_regSet_multi(auto_pat_off, size);
    }
    SHDISP_TRACE("out\n");
}



/* ------------------------------------------------------------------------- */
/*      shdisp_clmr_probe                                                    */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_probe(struct platform_device *pdev)
{
#ifdef CONFIG_OF
    struct resource *res;
    int rc = SHDISP_RESULT_SUCCESS;

    SHDISP_TRACE("in pdev = 0x%p\n", pdev );

    shdisp_clmr_ctrl.pdev = pdev;
    if (pdev) {
        shdisp_clmr_ctrl.core_reg_gpio = of_get_named_gpio(shdisp_clmr_ctrl.pdev->dev.of_node,
                         "shdisp_clmr,reg-gpio", 0);

        if (!gpio_is_valid(shdisp_clmr_ctrl.core_reg_gpio)) {
            SHDISP_ERR("gpio resource error!!\n");
            rc = SHDISP_RESULT_SUCCESS;
            goto probe_done;
        }

        res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
        if (!res) {
            SHDISP_ERR("irq resouce err!!\n");
            rc = SHDISP_RESULT_SUCCESS;
            goto probe_done;
        } else {
            shdisp_clmr_irq = res->start;
        }

    }

probe_done:
    SHDISP_TRACE("out rc = %d\n", rc );

    return rc;
#else
    return SHDISP_RESULT_SUCCESS;
#endif /* CONFIG_OF */
}


/* ------------------------------------------------------------------------- */
/*      shdisp_clmr_remove                                                   */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_remove(struct platform_device *pdev)
{


    return SHDISP_RESULT_SUCCESS;
}

#ifdef CONFIG_OF
static const struct of_device_id shdisp_clmr_dt_match[] = {
    { .compatible = "sharp,shdisp_clmr", } ,
    {}
};
#else
#define shdisp_clmr_dt_match NULL;
#endif /* CONFIG_OF */

static struct platform_driver shdisp_clmr_driver = {
    .probe = shdisp_clmr_probe,
    .remove = shdisp_clmr_remove,
    .shutdown = NULL,
    .driver = {
        /*
         * Driver name must match the device name added in
         * platform.c.
         */
        .name = "shdisp_clmr",
        .of_match_table = shdisp_clmr_dt_match,
    },
};

/* ------------------------------------------------------------------------- */
/*      shdisp_clmr_register_driver                                          */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_register_driver(void)
{
    return platform_driver_register(&shdisp_clmr_driver);
}

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_recover_for_extraordinary                                */
/*---------------------------------------------------------------------------*/
int shdisp_clmr_recover_for_extraordinary(void)
{
    int ret = SHDISP_RESULT_SUCCESS;
    int retry, count;
    unsigned char buf[4] = {0};

    SHDISP_TRACE("in\n");

    shdisp_SYS_FWCMD_set_timeoutexception(0);

    shdisp_clmr_arm_reset();

    count = ARRAY_SIZE(clock_setting1) - 1;
    shdisp_clmr_regSet(&clock_setting1[count]);

    for (retry = 0; retry < CLMR_RETRY; retry++) {
        shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_REG_PLLSTAT, NULL, 0, buf, sizeof(buf));
        if ((buf[3] & 0x02) == 0x02) {
            break;
        }
        shdisp_SYS_delay_us(500);
    }

    if (retry >= CLMR_RETRY) {
        SHDISP_ERR("PLL_LOCK timeout!!\n");
        return SHDISP_RESULT_FAILURE;
    }

    for (count = 1; count < 3; count++) {
        shdisp_clmr_regSet(&clock_setting2[count]);
    }

    shdisp_clmr_arm_init();

    ret = shdisp_clmr_arm_sram2fw();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("fw_boot failed.\n");
        return SHDISP_CLMR_PWRON_RESULT_BOOTFW_ERR;
    }

    shdisp_clmr_arm_boot();
    ret = shdisp_clmr_wait4fw_boot_comp();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("fw_boot timeout.\n");
        return SHDISP_RESULT_FAILURE;
    }

    ret = shdisp_clmr_api_fw_bdic_set_param();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("fw_bdic_set_param failed.\n");
        return SHDISP_RESULT_FAILURE;
    }

    shdisp_panel_API_request_RateCtrl(1, SHDISP_PANEL_RATE_60_0, SHDISP_PANEL_RATE_1);

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/*---------------------------------------------------------------------------*/
/*      debug command                                                        */
/*---------------------------------------------------------------------------*/
#define CALI_REG_DUMP       1
#define CALI_CUST_DUMP      2
#define CALI_ALL_DUMP       3
#define CALI_FW_DUMP        4
#define CALI_VER_DUMP       5

typedef struct {
    unsigned short addr;
    char *regName;
} clmr_reg_t;

const static clmr_reg_t clmr_reg[] = {
    {SHDISP_CLMR_REG_SYSCTL,                "SYSCTL"},
    {SHDISP_CLMR_REG_MCLKDIV,               "MCLKDIV"},
    {SHDISP_CLMR_REG_XCLKDIV,               "XCLKDIV"},
    {SHDISP_CLMR_REG_REGDIV,                "REGDIV"},
    {SHDISP_CLMR_REG_REFDIV,                "REFDIV"},
    {SHDISP_CLMR_REG_VRMDIV,                "VRMDIV"},
    {SHDISP_CLMR_REG_PREDIV,                "PREDIV"},
    {SHDISP_CLMR_REG_PTGDIV,                "PTGDIV"},
    {SHDISP_CLMR_REG_LAYDIV,                "LAYDIV"},
    {SHDISP_CLMR_REG_TPSTGDIV,              "TPSTGDIV"},
    {SHDISP_CLMR_REG_TCONTGDIV,             "TCONTGDIV"},
    {SHDISP_CLMR_REG_TXMPDIV,               "TXMPDIV"},
    {SHDISP_CLMR_REG_RXMPDIV,               "RXMPDIV"},
    {SHDISP_CLMR_REG_CALDIV,                "CALDIV"},
    {SHDISP_CLMR_REG_PWMDIV,                "PWMDIV"},
    {SHDISP_CLMR_REG_LUXDIV,                "LUXDIV"},
    {SHDISP_CLMR_REG_ARMTMRDIV,             "ARMTMRDIV"},
    {SHDISP_CLMR_REG_MOSRADIV,              "MOSRADIV"},
    {SHDISP_CLMR_REG_I2CDIV,                "I2CDIV"},
    {SHDISP_CLMR_REG_GPDIV,                 "GPDIV"},
    {SHDISP_CLMR_REG_CLKSYS,                "CLKSYS"},
    {SHDISP_CLMR_REG_CLKSYS2,               "CLKSYS2"},
    {SHDISP_CLMR_REG_CLKSYS3,               "CLKSYS3"},

    {SHDISP_CLMR_REG_CLKSELMASK,            "CLKSELMASK"},

    {SHDISP_CLMR_REG_PLL0CTL,               "PLL0CTL"},
    {SHDISP_CLMR_REG_PLL0CTL2,              "PLL0CTL2"},
    {SHDISP_CLMR_REG_PLL0CTL3,              "PLL0CTL3"},

    {SHDISP_CLMR_REG_PLL1CTL,               "PLL1CTL"},
    {SHDISP_CLMR_REG_PLL1CTL2,              "PLL1CTL2"},
    {SHDISP_CLMR_REG_PLL1CTL3,              "PLL1CTL3"},

    {SHDISP_CLMR_REG_PLLSTAT,               "PLLSTAT"},

    {SHDISP_CLMR_REG_OSCCTL,                "OSCCTL"},
    {SHDISP_CLMR_REG_OSCCTL2,               "OSCCTL2"},

    {SHDISP_CLMR_REG_INTR,                  "INTR"},
    {SHDISP_CLMR_REG_INTM,                  "INTM"},
    {SHDISP_CLMR_REG_INTRAW,                "INTRAW"},

    {SHDISP_CLMR_REG_INTSET1,               "INTSET1"},
    {SHDISP_CLMR_REG_INTSET2,               "INTSET2"},

    {SHDISP_CLMR_REG_SETINT1,               "SETINT1"},
    {SHDISP_CLMR_REG_SETINT2,               "SETINT2"},

    {SHDISP_CLMR_REG_INFOREG0,              "INFOREG0"},
    {SHDISP_CLMR_REG_INFOREG1,              "INFOREG1"},
    {SHDISP_CLMR_REG_INFOREG2,              "INFOREG2"},
    {SHDISP_CLMR_REG_INFOREG3,              "INFOREG3"},

    {SHDISP_CLMR_REG_SOFTRESET,             "SOFTRESET"},

    {SHDISP_CLMR_REG_DEVCODE,               "DEVCODE"},
    {SHDISP_CLMR_REG_SUBDEVCODE,            "SUBDEVCODE"},

    {SHDISP_CLMR_REG_REFCTL,                "REFCTL"},

    {SHDISP_CLMR_REG_CALCNTCTL,             "CALCNTCTL"},
    {SHDISP_CLMR_REG_CALCNTA,               "CALCNTA"},
    {SHDISP_CLMR_REG_CALCNTB,               "CALCNTB"},

    {SHDISP_CLMR_REG_PLLONCTL,              "PLLONCTL"},
    {SHDISP_CLMR_REG_PLLONCTLREG,           "PLLONCTLREG"},

    {SHDISP_CLMR_REG_EDRAMERR,              "EDRAMERR"},
    {SHDISP_CLMR_REG_EDRAMERRMASK,          "EDRAMERRMASK"},
    {SHDISP_CLMR_REG_EDRAMCONF,             "EDRAMCONF"},

    {SHDISP_CLMR_REG_VRAMMODECTL,           "VRAMMODECTL"},

    {SHDISP_CLMR_REG_CSTMSYS,               "CSTMSYS"},
    {SHDISP_CLMR_REG_TRVHW,                 "TRVHW"},

    {SHDISP_CLMR_REG_GIOSEL,                "GIOSEL"},
    {SHDISP_CLMR_REG_PFSEL,                 "PFSEL"},
    {SHDISP_CLMR_REG_DSCTL,                 "DSCTL"},

    {SHDISP_CLMR_REG_TSCTL,                 "TSCTL"},

    {SHDISP_CLMR_REG_RXBUFCTL,              "RXBUFCTL"},
    {SHDISP_CLMR_REG_RXBUFCTL2,             "RXBUFCTL2"},

    {SHDISP_CLMR_REG_TXBUFCTL,              "TXBUFCTL"},
    {SHDISP_CLMR_REG_TXBUFCTL2,             "TXBUFCTL2"},
    {SHDISP_CLMR_REG_TXBUFCTL3,             "TXBUFCTL3"},

    {SHDISP_CLMR_REG_MDRMSYS,               "MDRMSYS"},
    {SHDISP_CLMR_REG_MDRMCTL1,              "MDRMCTL1"},
    {SHDISP_CLMR_REG_MDRMCTL2,              "MDRMCTL2"},
    {SHDISP_CLMR_REG_MDRMDPHYCL,            "MDRMDPHYCL"},
    {SHDISP_CLMR_REG_MDRMDPHYDL,            "MDRMDPHYDL"},
    {SHDISP_CLMR_REG_MDRMDPHYCLCTL,         "MDRMDPHYCLCTL"},
    {SHDISP_CLMR_REG_MDRMDPHYDLCTL1,        "MDRMDPHYDLCTL1"},
    {SHDISP_CLMR_REG_MDRMDPHYDLCTL2,        "MDRMDPHYDLCTL2"},
    {SHDISP_CLMR_REG_MDRMTEST1,             "MDRMTEST1"},
    {SHDISP_CLMR_REG_MDRMTEST2,             "MDRMTEST2"},

    {SHDISP_CLMR_REG_MDRMERRREPSTAT,        "MDRMERRREPSTAT"},
    {SHDISP_CLMR_REG_MDRMERRREPMASK,        "MDRMERRREPMASK"},

    {SHDISP_CLMR_REG_TXSYS,                 "TXSYS"},
    {SHDISP_CLMR_REG_MDTMCTL,               "MDTMCTL"},
    {SHDISP_CLMR_REG_MDTMCTL2,              "MDTMCTL2"},
    {SHDISP_CLMR_REG_MDTMCMDCTL,            "MDTMCMDCTL"},
    {SHDISP_CLMR_REG_MDTMCMDADR,            "MDTMCMDADR"},
    {SHDISP_CLMR_REG_MDTMCMDDATA,           "MDTMCMDDATA"},
    {SHDISP_CLMR_REG_MDTMTRIG,              "MDTMTRIG"},

    {SHDISP_CLMR_REG_MDTMHSABYTE,           "MDTMHSABYTE"},
    {SHDISP_CLMR_REG_MDTMHBPBYTE,           "MDTMHBPBYTE"},
    {SHDISP_CLMR_REG_MDTMHFPBYTE,           "MDTMHFPBYTE"},
    {SHDISP_CLMR_REG_MDTMHBLBYTE,           "MDTMHBLBYTE"},
    {SHDISP_CLMR_REG_MDTMHWBYTE,            "MDTMHWBYTE"},

    {SHDISP_CLMR_REG_MDTMTHB,               "MDTMTHB"},
    {SHDISP_CLMR_REG_MDTMTHW,               "MDTMTHW"},
    {SHDISP_CLMR_REG_MDTMTVB,               "MDTMTVB"},
    {SHDISP_CLMR_REG_MDTMTVW,               "MDTMTVW"},
    {SHDISP_CLMR_REG_MDTMTACTL,             "MDTMTACTL"},

    {SHDISP_CLMR_REG_MDTMCLKCTL,            "MDTMCLKCTL"},

    {SHDISP_CLMR_REG_MDTMCTCTL,             "MDTMCTCTL"},
    {SHDISP_CLMR_REG_MDTMCTXTIME,           "MDTMCTXTIME"},
    {SHDISP_CLMR_REG_MDTMCTYTIME,           "MDTMCTYTIME"},

    {SHDISP_CLMR_REG_MDTMDPHYCL1,           "MDTMDPHYCL1"},
    {SHDISP_CLMR_REG_MDTMDPHYCL2,           "MDTMDPHYCL2"},
    {SHDISP_CLMR_REG_MDTMDPHYDL1,           "MDTMDPHYDL1"},
    {SHDISP_CLMR_REG_MDTMDPHYDL2,           "MDTMDPHYDL2"},

    {SHDISP_CLMR_REG_MDTMLPRXSTAT,          "MDTMLPRXSTAT"},
    {SHDISP_CLMR_REG_MDTMLPRXMASK,          "MDTMLPRXMASK"},
    {SHDISP_CLMR_REG_MDTMLPRXHDR,           "MDTMLPRXHDR"},
    {SHDISP_CLMR_REG_MDTMLPRXADR,           "MDTMLPRXADR"},
    {SHDISP_CLMR_REG_MDTMLPRXDATA,          "MDTMLPRXDATA"},

    {SHDISP_CLMR_REG_MDTM2CTL,              "MDTM2CTL"},
    {SHDISP_CLMR_REG_MDTM2CTL2,             "MDTM2CTL2"},

    {SHDISP_CLMR_REG_MDTM2CMDCTL,           "MDTM2CMDCTL"},
    {SHDISP_CLMR_REG_MDTM2CMDADR,           "MDTM2CMDADR"},
    {SHDISP_CLMR_REG_MDTM2CMDDATA,          "MDTM2CMDDATA"},
    {SHDISP_CLMR_REG_MDTM2TRIG,             "MDTM2TRIG"},
    {SHDISP_CLMR_REG_MDTM2HSABYTE,          "MDTM2HSABYTE"},
    {SHDISP_CLMR_REG_MDTM2HBPBYTE,          "MDTM2HBPBYTE"},
    {SHDISP_CLMR_REG_MDTM2HFPBYTE,          "MDTM2HFPBYTE"},
    {SHDISP_CLMR_REG_MDTM2HBLBYTE,          "MDTM2HBLBYTE"},
    {SHDISP_CLMR_REG_MDTM2HWBYTE,           "MDTM2HWBYTE"},

    {SHDISP_CLMR_REG_MDTM2THB,              "MDTM2THB"},
    {SHDISP_CLMR_REG_MDTM2THW,              "MDTM2THW"},
    {SHDISP_CLMR_REG_MDTM2TVB,              "MDTM2TVB"},
    {SHDISP_CLMR_REG_MDTM2TVW,              "MDTM2TVW"},
    {SHDISP_CLMR_REG_MDTM2TACTL,            "MDTM2TACTL"},

    {SHDISP_CLMR_REG_MDTM2CLKCTL,           "MDTM2CLKCTL"},

    {SHDISP_CLMR_REG_MDTM2CTCTL,            "MDTM2CTCTL"},
    {SHDISP_CLMR_REG_MDTM2CTXTIME,          "MDTM2CTXTIME"},
    {SHDISP_CLMR_REG_MDTM2CTYTIME,          "MDTM2CTYTIME"},

    {SHDISP_CLMR_REG_MDTM2DPHYCL1,          "MDTM2DPHYCL1"},
    {SHDISP_CLMR_REG_MDTM2DPHYCL2,          "MDTM2DPHYCL2"},
    {SHDISP_CLMR_REG_MDTM2DPHYDL1,          "MDTM2DPHYDL1"},
    {SHDISP_CLMR_REG_MDTM2DPHYDL2,          "MDTM2DPHYDL2"},
    {SHDISP_CLMR_REG_MDTM2LPRXSTAT,         "MDTM2LPRXSTAT"},
    {SHDISP_CLMR_REG_MDTM2LPRXMASK,         "MDTM2LPRXMASK"},

    {SHDISP_CLMR_REG_MDTM2LPRXHDR,          "MDTM2LPRXHDR"},
    {SHDISP_CLMR_REG_MDTM2LPRXADR,          "MDTM2LPRXADR"},
    {SHDISP_CLMR_REG_MDTM2LPRXDATA,         "MDTM2LPRXDATA"},

    {SHDISP_CLMR_REG_HOSTSYS,               "HOSTSYS"},
    {SHDISP_CLMR_REG_HOSTCTL,               "HOSTCTL"},
    {SHDISP_CLMR_REG_HOSTASX,               "HOSTASX"},
    {SHDISP_CLMR_REG_HOSTAEX,               "HOSTAEX"},
    {SHDISP_CLMR_REG_HOSTASY,               "HOSTASY"},
    {SHDISP_CLMR_REG_HOSTAEY,               "HOSTAEY"},
    {SHDISP_CLMR_REG_HOSTHW,                "HOSTHW"},
    {SHDISP_CLMR_REG_HOSTBASE,              "HOSTBASE"},

    {SHDISP_CLMR_REG_PREMIFCTL,             "PREMIFCTL"},
    {SHDISP_CLMR_REG_PRECSX,                "PRECSX"},
    {SHDISP_CLMR_REG_PRECSY,                "PRECSY"},
    {SHDISP_CLMR_REG_PRECEX,                "PRECEX"},
    {SHDISP_CLMR_REG_PRECEY,                "PRECEY"},
    {SHDISP_CLMR_REG_PREXSIZE,              "PREXSIZE"},
    {SHDISP_CLMR_REG_PREYSIZE,              "PREYSIZE"},
    {SHDISP_CLMR_REG_PREMOCOECTL1,          "PREMOCOECTL1"},
    {SHDISP_CLMR_REG_PREMOCOECTL2,          "PREMOCOECTL2"},
    {SHDISP_CLMR_REG_PREMOCOECTL3,          "PREMOCOECTL3"},
    {SHDISP_CLMR_REG_PREMOCOECTL4,          "PREMOCOECTL4"},
    {SHDISP_CLMR_REG_PREMOCOETBL,           "PREMOCOETBL"},
    {SHDISP_CLMR_REG_PREMOCOETBH,           "PREMOCOETBH"},
    {SHDISP_CLMR_REG_PREVRAMHW,             "PREVRAMHW"},
    {SHDISP_CLMR_REG_PREVRAMBASE,           "PREVRAMBASE"},
    {SHDISP_CLMR_REG_PREINTCTL,             "PREINTCTL"},
    {SHDISP_CLMR_REG_PREINT1X,              "PREINT1X"},
    {SHDISP_CLMR_REG_PREINT1Y,              "PREINT1Y"},
    {SHDISP_CLMR_REG_PREINT2X,              "PREINT2X"},
    {SHDISP_CLMR_REG_PREINT2Y,              "PREINT2Y"},
    {SHDISP_CLMR_REG_PREFMODECTL,           "PREFMODECTL"},
    {SHDISP_CLMR_REG_PREFSIZE,              "PREFSIZE"},

    {SHDISP_CLMR_REG_PREXSTAT,              "PREXSTAT"},
    {SHDISP_CLMR_REG_PREYSTAT,              "PREYSTAT"},

    {SHDISP_CLMR_REG_NACOCTL,               "NACOCTL"},

    {SHDISP_CLMR_REG_PRESYS,                "PRESYS"},

    {SHDISP_CLMR_REG_PSTCTL,                "PSTCTL"},
    {SHDISP_CLMR_REG_PSTVALTRAN,            "PSTVALTRAN"},
    {SHDISP_CLMR_REG_PTGCTL,                "PTGCTL"},
    {SHDISP_CLMR_REG_PTGHP,                 "PTGHP"},
    {SHDISP_CLMR_REG_PTGHB,                 "PTGHB"},
    {SHDISP_CLMR_REG_PTGHW,                 "PTGHW"},
    {SHDISP_CLMR_REG_PTGHF,                 "PTGHF"},
    {SHDISP_CLMR_REG_PTGVP,                 "PTGVP"},
    {SHDISP_CLMR_REG_PTGVB,                 "PTGVB"},
    {SHDISP_CLMR_REG_PTGVW,                 "PTGVW"},
    {SHDISP_CLMR_REG_PTGVF,                 "PTGVF"},
    {SHDISP_CLMR_REG_PTGSTEPCTL,            "PTGSTEPCTL"},
    {SHDISP_CLMR_REG_PTGVFS0,               "PTGVFS0"},
    {SHDISP_CLMR_REG_PTGVFS1,               "PTGVFS1"},
    {SHDISP_CLMR_REG_PTGVFS2,               "PTGVFS2"},
    {SHDISP_CLMR_REG_PTGVFS3,               "PTGVFS3"},
    {SHDISP_CLMR_REG_PTGVFS4,               "PTGVFS4"},
    {SHDISP_CLMR_REG_PTGVFS5,               "PTGVFS5"},
    {SHDISP_CLMR_REG_PTGREFCTL,             "PTGREFCTL"},
    {SHDISP_CLMR_REG_PTGOUTDLY,             "PTGOUTDLY"},
    {SHDISP_CLMR_REG_PTGOUTDLYREF,          "PTGOUTDLYREF"},
    {SHDISP_CLMR_REG_PTGDANGERSTART,        "PTGDANGERSTART"},
    {SHDISP_CLMR_REG_PTGDANGEREND,          "PTGDANGEREND"},
    {SHDISP_CLMR_REG_PTGPVINTLN1,           "PTGPVINTLN1"},
    {SHDISP_CLMR_REG_PTGPVINTLN2,           "PTGPVINTLN2"},
    {SHDISP_CLMR_REG_PTGVBLKCTL,            "PTGVBLKCTL"},
    {SHDISP_CLMR_REG_PTGVBLKS,              "PTGVBLKS"},
    {SHDISP_CLMR_REG_PTGVBLKE,              "PTGVBLKE"},
    {SHDISP_CLMR_REG_PTGDCSTECTL,           "PTGDCSTECTL"},
    {SHDISP_CLMR_REG_PTGDCSTESCANLINE,      "PTGDCSTESCANLINE"},
    {SHDISP_CLMR_REG_PTGTPSYNCCTL,          "PTGTPSYNCCTL"},
    {SHDISP_CLMR_REG_PTGTPSYNCS,            "PTGTPSYNCS"},
    {SHDISP_CLMR_REG_PTGTPSYNCE,            "PTGTPSYNCE"},
    {SHDISP_CLMR_REG_PTGSLOWVFCTL,          "PTGSLOWVFCTL"},
    {SHDISP_CLMR_REG_PTGSLOWVFMAX,          "PTGSLOWVFMAX"},
    {SHDISP_CLMR_REG_PTGSLOWVFINTCNT1,      "PTGSLOWVFINTCNT1"},
    {SHDISP_CLMR_REG_PTGSLOWVFINTCNT2,      "PTGSLOWVFINTCNT2"},
    {SHDISP_CLMR_REG_PTGSLOWVFDRVPWRLOW,    "PTGSLOWVFDRVPWRLOW"},
    {SHDISP_CLMR_REG_PTGSLOWVFDRVPWRHI,     "PTGSLOWVFDRVPWRHI"},
    {SHDISP_CLMR_REG_PTGSLOWTPSYNCCTL,      "PTGSLOWTPSYNCCTL"},
    {SHDISP_CLMR_REG_PTGSLOWTPSYNCMAX,      "PTGSLOWTPSYNCMAX"},
    {SHDISP_CLMR_REG_PTGSLOWTPSYNCHI,       "PTGSLOWTPSYNCHI"},
    {SHDISP_CLMR_REG_PTGSLOWTPSYNCLOW,      "PTGSLOWTPSYNCLOW"},
    {SHDISP_CLMR_REG_PTGSLOWTECTL,          "PTGSLOWTECTL"},
    {SHDISP_CLMR_REG_PTGSLOWTEMAX,          "PTGSLOWTEMAX"},
    {SHDISP_CLMR_REG_PTGSLOWTEHI,           "PTGSLOWTEHI"},
    {SHDISP_CLMR_REG_PTGSLOWTELOW,          "PTGSLOWTELOW"},

    {SHDISP_CLMR_REG_PTGMIFCTL,             "PTGMIFCTL"},
    {SHDISP_CLMR_REG_PTGSRCXSIZE,           "PTGSRCXSIZE"},
    {SHDISP_CLMR_REG_PTGSRCYSIZE,           "PTGSRCYSIZE"},
    {SHDISP_CLMR_REG_PTGDESXSIZE,           "PTGDESXSIZE"},
    {SHDISP_CLMR_REG_PTGDESYSIZE,           "PTGDESYSIZE"},
    {SHDISP_CLMR_REG_PTGMOCODCTL1,          "PTGMOCODCTL1"},
    {SHDISP_CLMR_REG_PTGMOCODCTL2,          "PTGMOCODCTL2"},
    {SHDISP_CLMR_REG_PTGMOCODCTL3,          "PTGMOCODCTL3"},
    {SHDISP_CLMR_REG_PTGMOCODCTL4,          "PTGMOCODCTL4"},
    {SHDISP_CLMR_REG_PTGMOCODTBL,           "PTGMOCODTBL"},
    {SHDISP_CLMR_REG_PTGMOCODTBH,           "PTGMOCODTBH"},

    {SHDISP_CLMR_REG_PTGFILCTL,             "PTGFILCTL"},
    {SHDISP_CLMR_REG_PTGFILADR,             "PTGFILADR"},
    {SHDISP_CLMR_REG_PTGFILDATA,            "PTGFILDATA"},
    {SHDISP_CLMR_REG_PTGFMODECTL,           "PTGFMODECTL"},
    {SHDISP_CLMR_REG_PTGFSIZE,              "PTGFSIZE"},
    {SHDISP_CLMR_REG_PTGPSCX1CTL,           "PTGPSCX1CTL"},
    {SHDISP_CLMR_REG_PTGPSCX1START,         "PTGPSCX1START"},
    {SHDISP_CLMR_REG_PTGPSCX1END,           "PTGPSCX1END"},
    {SHDISP_CLMR_REG_PTGPSCX2CTL,           "PTGPSCX2CTL"},
    {SHDISP_CLMR_REG_PTGPSCX2START,         "PTGPSCX2START"},
    {SHDISP_CLMR_REG_PTGPSCX2END,           "PTGPSCX2END"},
    {SHDISP_CLMR_REG_PTGPSCXERAT,           "PTGPSCXERAT"},
    {SHDISP_CLMR_REG_PTGPSCYCTL,            "PTGPSCYCTL"},
    {SHDISP_CLMR_REG_PTGPSCYSTART,          "PTGPSCYSTART"},
    {SHDISP_CLMR_REG_PTGPSCYEND,            "PTGPSCYEND"},
    {SHDISP_CLMR_REG_PTGPSCYERAT,           "PTGPSCYERAT"},
    {SHDISP_CLMR_REG_PTGPSCRATADR,          "PTGPSCRATADR"},
    {SHDISP_CLMR_REG_PTGPSCRATDAT,          "PTGPSCRATDAT"},
    {SHDISP_CLMR_REG_PTGCUTCTL,             "PTGCUTCTL"},
    {SHDISP_CLMR_REG_PTGCUTCSX,             "PTGCUTCSX"},
    {SHDISP_CLMR_REG_PTGCUTCEX,             "PTGCUTCEX"},
    {SHDISP_CLMR_REG_PTGCUTCSY,             "PTGCUTCSY"},
    {SHDISP_CLMR_REG_PTGCUTCEY,             "PTGCUTCEY"},
    {SHDISP_CLMR_REG_PTGYOFFSET,            "PTGYOFFSET"},
    {SHDISP_CLMR_REG_PTGVRAMHW,             "PTGVRAMHW"},
    {SHDISP_CLMR_REG_PTGVRAMBASE,           "PTGVRAMBASE"},
    {SHDISP_CLMR_REG_PTGLAYRSTX,            "PTGLAYRSTX"},
    {SHDISP_CLMR_REG_PTGLAYRSTY,            "PTGLAYRSTY"},

    {SHDISP_CLMR_REG_PTGXSTAT,              "PTGXSTAT"},
    {SHDISP_CLMR_REG_PTGYSTAT,              "PTGYSTAT"},
    {SHDISP_CLMR_REG_PTGPREFSTAT,           "PTGPREFSTAT"},
    {SHDISP_CLMR_REG_PTGHREFSTAT,           "PTGHREFSTAT"},
    {SHDISP_CLMR_REG_PTGFIFOSTAT,           "PTGFIFOSTAT"},
    {SHDISP_CLMR_REG_PTGSTEPSTAT,           "PTGSTEPSTAT"},
    {SHDISP_CLMR_REG_PTGVBLKSTAT,           "PTGVBLKSTAT"},
    {SHDISP_CLMR_REG_PTGSLOWSTAT,           "PTGSLOWSTAT"},
    {SHDISP_CLMR_REG_PTGSLOWCNTSTAT,        "PTGSLOWCNTSTAT"},
    {SHDISP_CLMR_REG_PTGSLOWTPCNTSTAT,      "PTGSLOWTPCNTSTAT"},
    {SHDISP_CLMR_REG_PTGSLOWTECNTSTAT,      "PTGSLOWTECNTSTAT"},

    {SHDISP_CLMR_REG_PSTSYS,                "PSTSYS"},

    {SHDISP_CLMR_REG_TCSYS,                 "TCSYS"},
    {SHDISP_CLMR_REG_TCCTL,                 "TCCTL"},
    {SHDISP_CLMR_REG_TCTHP,                 "TCTHP"},
    {SHDISP_CLMR_REG_TCTHB,                 "TCTHB"},
    {SHDISP_CLMR_REG_TCTHW0,                "TCTHW0"},
    {SHDISP_CLMR_REG_TCTHW1,                "TCTHW1"},
    {SHDISP_CLMR_REG_TCDELAY,               "TCDELAY"},
    {SHDISP_CLMR_REG_TCTHFMIN,              "TCTHFMIN"},
    {SHDISP_CLMR_REG_TCHW0OFS,              "TCHW0OFS"},
    {SHDISP_CLMR_REG_TCHW1OFS,              "TCHW1OFS"},
    {SHDISP_CLMR_REG_TCTVP,                 "TCTVP"},

    {SHDISP_CLMR_REG_ARMHOSTSYS,            "ARMHOSTSYS"},
    {SHDISP_CLMR_REG_ARMDEBEN,              "ARMDEBEN"},
    {SHDISP_CLMR_REG_ARMCONTCLKEN,          "ARMCONTCLKEN"},
    {SHDISP_CLMR_REG_ARMTMRCLKEN,           "ARMTMRCLKEN"},
    {SHDISP_CLMR_REG_ARMSLPCLKEN,           "ARMSLPCLKEN"},
    {SHDISP_CLMR_REG_ARMTIM0SYS,            "ARMTIM0SYS"},
    {SHDISP_CLMR_REG_ARMTIM0CNT,            "ARMTIM0CNT"},
    {SHDISP_CLMR_REG_ARMTIM1SYS,            "ARMTIM1SYS"},
    {SHDISP_CLMR_REG_ARMTIM1CNT,            "ARMTIM1CNT"},
    {SHDISP_CLMR_REG_ARMDMA,                "ARMDMA"},
    {SHDISP_CLMR_REG_ARMSRAMBASE,           "ARMSRAMBASE"},
    {SHDISP_CLMR_REG_ARMEDRAMBASE,          "ARMEDRAMBASE"},
    {SHDISP_CLMR_REG_ARMEDRAMSTART,         "ARMEDRAMSTART"},
    {SHDISP_CLMR_REG_ARMDMASIZE,            "ARMDMASIZE"},
    {SHDISP_CLMR_REG_ARMDMAHW,              "ARMDMAHW"},

    {SHDISP_CLMR_REG_ARMUARTCTL,            "ARMUARTCTL"},
    {SHDISP_CLMR_REG_ARMUARTBORT,           "ARMUARTBORT"},
    {SHDISP_CLMR_REG_ARMUARTSTAT,           "ARMUARTSTAT"},
    {SHDISP_CLMR_REG_ARMUARTDAT,            "ARMUARTDAT"},

    {SHDISP_CLMR_REG_ARMSRAMCTL,            "ARMSRAMCTL"},
    {SHDISP_CLMR_REG_ARMSRAMADR,            "ARMSRAMADR"},
    {SHDISP_CLMR_REG_ARMSRAMDAT,            "ARMSRAMDAT"},

    {SHDISP_CLMR_REG_ARMINTR,               "ARMINTR"},
    {SHDISP_CLMR_REG_ARMINTM,               "ARMINTM"},
    {SHDISP_CLMR_REG_ARMINTRAW,             "ARMINTRAW"},

    {SHDISP_CLMR_REG_ARMINTSET1,            "ARMINTSET1"},
    {SHDISP_CLMR_REG_ARMINTSET2,            "ARMINTSET2"},

    {SHDISP_CLMR_REG_ARMSETINT1,            "ARMSETINT1"},
    {SHDISP_CLMR_REG_ARMSETINT2,            "ARMSETINT2"},

    {SHDISP_CLMR_REG_ARMINFOREG0,           "ARMINFOREG0"},
    {SHDISP_CLMR_REG_ARMINFOREG1,           "ARMINFOREG1"},
    {SHDISP_CLMR_REG_ARMINFOREG2,           "ARMINFOREG2"},
    {SHDISP_CLMR_REG_ARMINFOREG3,           "ARMINFOREG3"},

    {SHDISP_CLMR_REG_ARMMMU0,               "ARMMMU0"},
    {SHDISP_CLMR_REG_ARMMMU1,               "ARMMMU1"},
    {SHDISP_CLMR_REG_ARMMMU2,               "ARMMMU2"},
    {SHDISP_CLMR_REG_ARMMMU3,               "ARMMMU3"},
    {SHDISP_CLMR_REG_ARMMMU4,               "ARMMMU4"},
    {SHDISP_CLMR_REG_ARMMMU5,               "ARMMMU5"},
    {SHDISP_CLMR_REG_ARMMMU6,               "ARMMMU6"},
    {SHDISP_CLMR_REG_ARMMMU7,               "ARMMMU7"},
    {SHDISP_CLMR_REG_ARMMMU8,               "ARMMMU8"},
    {SHDISP_CLMR_REG_ARMMMU9,               "ARMMMU9"},
    {SHDISP_CLMR_REG_ARMMMU10,              "ARMMMU10"},
    {SHDISP_CLMR_REG_ARMMMU11,              "ARMMMU11"},
    {SHDISP_CLMR_REG_ARMMMU12,              "ARMMMU12"},
    {SHDISP_CLMR_REG_ARMMMU13,              "ARMMMU13"},
    {SHDISP_CLMR_REG_ARMMMU14,              "ARMMMU14"},
    {SHDISP_CLMR_REG_ARMMMU15,              "ARMMMU15"},
    {SHDISP_CLMR_REG_ARMMMU16,              "ARMMMU16"},
    {SHDISP_CLMR_REG_ARMMMU17,              "ARMMMU17"},
    {SHDISP_CLMR_REG_ARMMMU18,              "ARMMMU18"},
    {SHDISP_CLMR_REG_ARMMMU19,              "ARMMMU19"},
    {SHDISP_CLMR_REG_ARMMMU20,              "ARMMMU20"},
    {SHDISP_CLMR_REG_ARMMMU21,              "ARMMMU21"},
    {SHDISP_CLMR_REG_ARMMMU22,              "ARMMMU22"},
    {SHDISP_CLMR_REG_ARMMMU23,              "ARMMMU23"},
    {SHDISP_CLMR_REG_ARMMMU24,              "ARMMMU24"},
    {SHDISP_CLMR_REG_ARMMMU25,              "ARMMMU25"},
    {SHDISP_CLMR_REG_ARMMMU26,              "ARMMMU26"},
    {SHDISP_CLMR_REG_ARMMMU27,              "ARMMMU27"},
    {SHDISP_CLMR_REG_ARMMMU28,              "ARMMMU28"},
    {SHDISP_CLMR_REG_ARMMMU29,              "ARMMMU29"},
    {SHDISP_CLMR_REG_ARMMMU30,              "ARMMMU30"},
    {SHDISP_CLMR_REG_ARMMMU31,              "ARMMMU31"},

    {SHDISP_CLMR_REG_MOSSYS,                "MOSSYS"},
    {SHDISP_CLMR_REG_MOSCTL,                "MOSCTL"},
    {SHDISP_CLMR_REG_MOSCTL2,               "MOSCTL2"},
    {SHDISP_CLMR_REG_MOSDBADR,              "MOSDBADR"},
    {SHDISP_CLMR_REG_MOSDBDAT,              "MOSDBDAT"},
    {SHDISP_CLMR_REG_MOSDBG1,               "MOSDBG1"},
    {SHDISP_CLMR_REG_MOSDBG2,               "MOSDBG2"},
    {SHDISP_CLMR_REG_MOSDBG3,               "MOSDBG3"},
    {SHDISP_CLMR_REG_MOSDBG4,               "MOSDBG4"},
    {SHDISP_CLMR_REG_MOSSRAMCTL,            "MOSSRAMCTL"},
    {SHDISP_CLMR_REG_MOSSRAMADR,            "MOSSRAMADR"},
    {SHDISP_CLMR_REG_MOSSRAMDAT,            "MOSSRAMDAT"},

    {SHDISP_CLMR_REG_I2CSYS,                "I2CSYS"},
    {SHDISP_CLMR_REG_I2CCTL,                "I2CCTL"},
    {SHDISP_CLMR_REG_I2CCTL2,               "I2CCTL2"},
    {SHDISP_CLMR_REG_I2CPWM,                "I2CPWM"},
    {SHDISP_CLMR_REG_I2CADR,                "I2CADR"},
    {SHDISP_CLMR_REG_I2CDRW0,               "I2CDRW0"},
    {SHDISP_CLMR_REG_I2CDRW1,               "I2CDRW1"},
    {SHDISP_CLMR_REG_I2CDRW2,               "I2CDRW2"},
    {SHDISP_CLMR_REG_I2CDRW3,               "I2CDRW3"},
    {SHDISP_CLMR_REG_I2CSTLIM,              "I2CSTLIM"},
    {SHDISP_CLMR_REG_I2CINTSTAT,            "I2CINTSTAT"},
    {SHDISP_CLMR_REG_I2CINTMASK,            "I2CINTMASK"},
    {SHDISP_CLMR_REG_I2CINTRAW,             "I2CINTRAW"},

    {SHDISP_CLMR_REG_EEPSYS,                "EEPSYS"},
    {SHDISP_CLMR_REG_EEPCTL,                "EEPCTL"},
    {SHDISP_CLMR_REG_EEPCTL2,               "EEPCTL2"},
    {SHDISP_CLMR_REG_EEPCTL3,               "EEPCTL3"},
    {SHDISP_CLMR_REG_EEPSTADR,              "EEPSTADR"},
    {SHDISP_CLMR_REG_EEPC32ADR,             "EEPC32ADR"},
    {SHDISP_CLMR_REG_EEPC16ADR,             "EEPC16ADR"},
    {SHDISP_CLMR_REG_EEPA32ADR,             "EEPA32ADR"},
    {SHDISP_CLMR_REG_EEPA16ADR,             "EEPA16ADR"},
    {SHDISP_CLMR_REG_EEPSTAT,               "EEPSTAT"},
    {SHDISP_CLMR_REG_EEPINTSTAT,            "EEPINTSTAT"},
    {SHDISP_CLMR_REG_EEPINTMASK,            "EEPINTMASK"},
    {SHDISP_CLMR_REG_EEPINTRAW,             "EEPINTRAW"},

    {SHDISP_CLMR_REG_GIO00,                 "GIO00"},
    {SHDISP_CLMR_REG_GIO01,                 "GIO01"},
    {SHDISP_CLMR_REG_GIO02,                 "GIO02"},
    {SHDISP_CLMR_REG_GIO03,                 "GIO03"},
    {SHDISP_CLMR_REG_GIO04,                 "GIO04"},
    {SHDISP_CLMR_REG_GIO05,                 "GIO05"},
    {SHDISP_CLMR_REG_GIO06,                 "GIO06"},
    {SHDISP_CLMR_REG_GIO07,                 "GIO07"},
    {SHDISP_CLMR_REG_GIO08,                 "GIO08"},
    {SHDISP_CLMR_REG_GIO09,                 "GIO09"},
    {SHDISP_CLMR_REG_GIO10,                 "GIO10"},
    {SHDISP_CLMR_REG_GIO11,                 "GIO11"},
    {SHDISP_CLMR_REG_GIO12,                 "GIO12"},
    {SHDISP_CLMR_REG_GIO13,                 "GIO13"},
    {SHDISP_CLMR_REG_GIO14,                 "GIO14"},

    {SHDISP_CLMR_REG_TESTMODE00,            "TESTMODE00"},
    {SHDISP_CLMR_REG_TESTMODE01,            "TESTMODE01"},
    {SHDISP_CLMR_REG_TESTMODE02,            "TESTMODE02"},
    {SHDISP_CLMR_REG_TESTMODE03,            "TESTMODE03"},
    {SHDISP_CLMR_REG_TESTMODE04,            "TESTMODE04"},
    {SHDISP_CLMR_REG_TESTMODE05,            "TESTMODE05"},
    {SHDISP_CLMR_REG_TESTMODE06,            "TESTMODE06"},
    {SHDISP_CLMR_REG_TESTMODE07,            "TESTMODE07"},
    {SHDISP_CLMR_REG_TESTMODE08,            "TESTMODE08"},
    {SHDISP_CLMR_REG_TESTMODE09,            "TESTMODE09"},
    {SHDISP_CLMR_REG_TESTMODE10,            "TESTMODE10"},
    {SHDISP_CLMR_REG_TESTMODE11,            "TESTMODE11"},
    {SHDISP_CLMR_REG_TESTMODE12,            "TESTMODE12"},
    {SHDISP_CLMR_REG_TESTMODE13,            "TESTMODE13"},
    {SHDISP_CLMR_REG_TESTMODE14,            "TESTMODE14"},
    {SHDISP_CLMR_REG_TESTMODE15,            "TESTMODE15"},

    { 0x4040,                               "TESTMODEREAD00"},
    { 0x4044,                               "TESTMODEREAD01"},
    { 0x4048,                               "TESTMODEREAD02"},
    { 0x404C,                               "TESTMODEREAD03"},
    { 0x4050,                               "TESTMODEREAD04"},
    { 0x4054,                               "TESTMODEREAD05"},
    { 0x4058,                               "TESTMODEREAD06"},
    { 0x405C,                               "TESTMODEREAD07"},
    { 0x4060,                               "TESTMODEREAD08"},
    { 0x4064,                               "TESTMODEREAD09"},
    { 0x4068,                               "TESTMODEREAD10"},
    { 0x406C,                               "TESTMODEREAD11"},
    { 0x4070,                               "TESTMODEREAD12"},
    { 0x4074,                               "TESTMODEREAD13"},
    { 0x4078,                               "TESTMODEREAD14"},
    { 0x407C,                               "TESTMODEREAD15"},
    { 0x4080,                               "TESTMODEREAD16"},
    { 0x4084,                               "TESTMODEREAD17"},
    { 0x4088,                               "TESTMODEREAD18"},
    { 0x408C,                               "TESTMODEREAD19"},
    { 0x4090,                               "TESTMODEREAD20"},
    { 0x4094,                               "TESTMODEREAD21"},
    { 0x4098,                               "TESTMODEREAD22"},
    { 0x409C,                               "TESTMODEREAD23"},
    { 0x40A0,                               "TESTMODEREAD24"},
    { 0x40A4,                               "TESTMODEREAD25"},
    { 0x40A8,                               "TESTMODEREAD26"},
    { 0x40AC,                               "TESTMODEREAD27"},
    { 0x40B0,                               "TESTMODEREAD28"},
    { 0x40B4,                               "TESTMODEREAD29"},
    { 0x40B8,                               "TESTMODEREAD30"},
    { 0x40BC,                               "TESTMODEREAD31"},
};

const static clmr_reg_t clmr_customReg[] = {
    { 0x8000,                                   "          "},
    {SHDISP_CLMR_CUST_AWHS,                     "AWHS      "},
    { 0x8008,                                   "          "},
    { 0x800C,                                   "          "},
    { 0x8010,                                   "          "},
    { 0x8014,                                   "          "},
    { 0x8018,                                   "          "},
    { 0x801C,                                   "          "},
    { 0x8020,                                   "          "},
    { 0x8024,                                   "          "},
    {SHDISP_CLMR_CUST_OUTPUTSEL,                "OUTPUTSEL "},
    {SHDISP_CLMR_CUST_VSPREGON,                 "VSPREGON  "},
    { 0x8034,                                   "          "},
    {SHDISP_CLMR_CUST_VSPCTRL1,                 "VSPCTRL1  "},
    {SHDISP_CLMR_CUST_VSPCTRL2,                 "VSPCTRL2  "},
    {SHDISP_CLMR_CUST_VSPCTRL3,                 "VSPCTRL3  "},
    { 0x8044,                                   "          "},
    {SHDISP_CLMR_CUST_VSPCTRL5,                 "VSPCTRL5  "},
    {SHDISP_CLMR_CUST_VSPCTRL6,                 "VSPCTRL6  "},
    { 0x8050,                                   "          "},
    {SHDISP_CLMR_CUST_VSPCTRL8,                 "VSPCTRL8  "},
    {SHDISP_CLMR_CUST_VSPCTRL9,                 "VSPCTRL9  "},
    { 0x805C,                                   "          "},
    {SHDISP_CLMR_CUST_INTSTAT0,                 "INTSTAT0  "},
    { 0x8064,                                   "          "},
    {SHDISP_CLMR_CUST_INTMASK0,                 "INTMASK0  "},
    { 0x806C,                                   "          "},
    {SHDISP_CLMR_CUST_INTSET0,                  "INTSET0   "},
    { 0x8074,                                   "          "},
    {SHDISP_CLMR_CUST_INTRST0,                  "INTRST0   "},
    { 0x807C,                                   "          "},
    { 0x8080,                                   "          "},
    { 0x8084,                                   "          "},
    { 0x8088,                                   "          "},
    { 0x808C,                                   "          "},
    {SHDISP_CLMR_CUST_STINT,                    "STINT     "},
    { 0x8094,                                   "          "},
    {SHDISP_CLMR_CUST_TUNE1,                    "TUNE1     "},
    { 0x809C,                                   "          "},
    {SHDISP_CLMR_CUST_STCNT1,                   "STCNT1    "},
    { 0x80A4,                                   "          "},
    { 0x80A8,                                   "          "},
    { 0x80AC,                                   "          "},
    { 0x80B0,                                   "          "},
    { 0x80B4,                                   "          "},
    { 0x80B8,                                   "          "},
    { 0x80BC,                                   "          "},
    { 0x80C0,                                   "          "},
    { 0x80C4,                                   "          "},
    { 0x80C8,                                   "          "},
    { 0x80CC,                                   "          "},
    { 0x80D0,                                   "          "},
    { 0x80D4,                                   "          "},
    { 0x80D8,                                   "          "},
    { 0x80DC,                                   "          "},
    { 0x80E0,                                   "          "},
    { 0x80E4,                                   "          "},
    { 0x80E8,                                   "          "},
    {SHDISP_CLMR_CUST_VSP_TEST,                 "VSP_TEST  "},
    { 0x80F0,                                   "          "},
    { 0x80F4,                                   "          "},
    {SHDISP_CLMR_CUST_CLKGATEOFF,               "CLKGATEOFF"},
    {SHDISP_CLMR_CUST_CUSTOM_INT,               "CUSTOM_INT"},

    {SHDISP_CLMR_CUST_LUXCTRL,                  "LUXCTRL   "},
    {SHDISP_CLMR_CUST_LUXDATA0,                 "LUXDATA0  "},
    {SHDISP_CLMR_CUST_LUXDATA1,                 "LUXDATA1  "},
    {SHDISP_CLMR_CUST_LUXNC,                    "LUXNC     "},
    { 0x8110,                                   "          "},
    { 0x8114,                                   "          "},
    { 0x8118,                                   "          "},
    { 0x811C,                                   "          "},
    { 0x8120,                                   "          "},
    { 0x8124,                                   "          "},
    { 0x8128,                                   "          "},
    { 0x812C,                                   "          "},
    { 0x8130,                                   "          "},
    { 0x8134,                                   "          "},
    { 0x8138,                                   "          "},
    { 0x813C,                                   "          "},
    {SHDISP_CLMR_CUST_NP1CTL,                   "NP1CTL    "},
    {SHDISP_CLMR_CUST_NP1RSLT,                  "NP1RSLT   "},
    {SHDISP_CLMR_CUST_NP1THRE,                  "NP1THRE   "},
    {SHDISP_CLMR_CUST_NP1DLY,                   "NP1DLY    "},
    {SHDISP_CLMR_CUST_NP1LONG,                  "NP1LONG   "},
    { 0x8154,                                   "          "},
    { 0x8158,                                   "          "},
    {SHDISP_CLMR_CUST_PWMCTRL,                  "PWMCTRL   "},
    {SHDISP_CLMR_CUST_PWMDLYSEL,                "PWMDLYSEL "},
    {SHDISP_CLMR_CUST_PWMOUT,                   "PWMOUT    "},
    { 0x8168,                                   "          "},
    { 0x816C,                                   "          "},
    { 0x8170,                                   "          "},
    { 0x8174,                                   "          "},
    { 0x8178,                                   "          "},
    { 0x817C,                                   "          "},
    {SHDISP_CLMR_CUST_SBLFMT,                   "SBLFMT    "},
    {SHDISP_CLMR_CUST_SBLSIZE0,                 "SBLSIZE0  "},
    {SHDISP_CLMR_CUST_SBLSIZE1,                 "SBLSIZE1  "},
    {SHDISP_CLMR_CUST_SBLSET0,                  "SBLSET0   "},
    {SHDISP_CLMR_CUST_SBLSET1,                  "SBLSET1   "},
    {SHDISP_CLMR_CUST_SBLSET2,                  "SBLSET2   "},
    {SHDISP_CLMR_CUST_SBLLUT_FI,                "SBLLUT_FI "},
    {SHDISP_CLMR_CUST_SBLLUT_CC,                "SBLLUT_CC "},
    {SHDISP_CLMR_CUST_APISET0,                  "APISET0   "},
    {SHDISP_CLMR_CUST_APISET1,                  "APISET1   "},
    {SHDISP_CLMR_CUST_APISET2,                  "APISET2   "},
    {SHDISP_CLMR_CUST_CALIBRATION,              "CALIBRATION"},
    {SHDISP_CLMR_CUST_DRC,                      "DRC       "},
    {SHDISP_CLMR_CUST_APIMON,                   "APIMON    "},
    {SHDISP_CLMR_CUST_ALCALIBLUT,               "ALCALIBLUT"},
    {SHDISP_CLMR_CUST_LOGO,                     "LOGO      "},
    {SHDISP_CLMR_CUST_SBLTEST,                  "SBLTEST   "},
    { 0x81C4,                                   "          "},
    { 0x81C8,                                   "          "},
    { 0x81CC,                                   "          "},
    { 0x81D0,                                   "          "},
    { 0x81D4,                                   "          "},
    { 0x81D8,                                   "          "},
    { 0x81DC,                                   "          "},
    { 0x81E0,                                   "          "},
    { 0x81E4,                                   "          "},
    { 0x81E8,                                   "          "},
    { 0x81EC,                                   "          "},
    { 0x81F0,                                   "          "},
    { 0x81F4,                                   "          "},
    { 0x81F8,                                   "          "},
    { 0x81FC,                                   "          "},

    {SHDISP_CLMR_CUST_EDIF,                     "EDIF      "},
    { 0x8204,                                   "          "},
    { 0x8208,                                   "          "},
    { 0x820C,                                   "          "},
    { 0x8210,                                   "          "},
    { 0x8214,                                   "          "},
    { 0x8218,                                   "          "},
    { 0x821C,                                   "          "},
    { 0x8220,                                   "          "},
    { 0x8224,                                   "          "},
    { 0x8228,                                   "          "},
    { 0x822C,                                   "          "},
    {SHDISP_CLMR_CUST_ITLC,                     "ITLC      "},
    { 0x8234,                                   "          "},
    { 0x8238,                                   "          "},
    { 0x823C,                                   "          "},
    {SHDISP_CLMR_CUST_Y2R_R2Y,                  "Y2R_R2Y   "},
    { 0x8244,                                   "          "},
    { 0x8248,                                   "          "},
    { 0x824C,                                   "          "},
    { 0x8250,                                   "          "},
    { 0x8254,                                   "          "},
    { 0x8258,                                   "          "},
    { 0x825C,                                   "          "},
    { 0x8260,                                   "          "},
    { 0x8264,                                   "          "},
    { 0x8268,                                   "          "},
    { 0x826C,                                   "          "},
    { 0x8270,                                   "          "},
    { 0x8274,                                   "          "},
    { 0x8278,                                   "          "},
    { 0x827C,                                   "          "},
    {SHDISP_CLMR_CUST_CPFAEXY,                  "CPFAEXY   "},
    {SHDISP_CLMR_CUST_CPFASXY,                  "CPFASXY   "},
    {SHDISP_CLMR_CUST_CPFCTRL0,                 "CPFCTRL0  "},
    {SHDISP_CLMR_CUST_CPFCTRL1,                 "CPFCTRL1  "},
    {SHDISP_CLMR_CUST_CPFCTRL2,                 "CPFCTRL2  "},
    {SHDISP_CLMR_CUST_CPFCTRL3,                 "CPFCTRL3  "},
    {SHDISP_CLMR_CUST_CPFSTRDAT0,               "CPFSTRDAT0"},
    {SHDISP_CLMR_CUST_CPFSTRDAT1,               "CPFSTRDAT1"},
    {SHDISP_CLMR_CUST_CPFSTRDAT2,               "CPFSTRDAT2"},
    {SHDISP_CLMR_CUST_CPFFIXLUT,                "CPFFIXLUT "},
    {SHDISP_CLMR_CUST_CPFLUTVFVAL0,             "CPFLUTVFVAL0"},
    {SHDISP_CLMR_CUST_CPFLUTVFVAL1,             "CPFLUTVFVAL1"},
    {SHDISP_CLMR_CUST_CPFLUTVFVAL2,             "CPFLUTVFVAL2"},
    { 0x82B4,                                   "          "},
    { 0x82B8,                                   "          "},
    { 0x82BC,                                   "          "},
    {SHDISP_CLMR_CUST_SPCOLCNT,                 "SPCOLCNT  "},
    {SHDISP_CLMR_CUST_SPCOLR,                   "SPCOLR    "},
    {SHDISP_CLMR_CUST_SPCOLG,                   "SPCOLG    "},
    {SHDISP_CLMR_CUST_SPCOLB,                   "SPCOLB    "},
    {SHDISP_CLMR_CUST_EWBPAT0,                  "EWBPAT0   "},
    {SHDISP_CLMR_CUST_EWBPAT1,                  "EWBPAT1   "},
    {SHDISP_CLMR_CUST_EWBPAT2,                  "EWBPAT2   "},
    {SHDISP_CLMR_CUST_EWBPAT3,                  "EWBPAT3   "},
    {SHDISP_CLMR_CUST_EWBLUTADR,                "EWBLUTADR "},
    {SHDISP_CLMR_CUST_EWBWRDAT,                 "EWBWRDAT  "},
    { 0x82E8,                                   "          "},
    {SHDISP_CLMR_CUST_EWBRDDAT0,                "EWBRDDAT0 "},
    {SHDISP_CLMR_CUST_EWBRDDAT1,                "EWBRDDAT1 "},
    {SHDISP_CLMR_CUST_EWBWREXEC,                "EWBWREXEC "},
    {SHDISP_CLMR_CUST_EWBEX,                    "EWBEX     "},
    { 0x82FC,                                   "          "},

    {SHDISP_CLMR_CUST_TRVASAXY0,                "TRVASAXY0 "},
    {SHDISP_CLMR_CUST_TRVAWHS0,                 "TRVAWHS0  "},
    {SHDISP_CLMR_CUST_TRVTEXADR0,               "TRVTEXADR0"},
    {SHDISP_CLMR_CUST_TRVTEXADR1,               "TRVTEXADR1"},
    {SHDISP_CLMR_CUST_TRVTEXADR2,               "TRVTEXADR2"},
    {SHDISP_CLMR_CUST_TRVTEXADR3,               "TRVTEXADR3"},
    {SHDISP_CLMR_CUST_TRVDATA,                  "TRVDATA   "},
    {SHDISP_CLMR_CUST_TRVLUTADR,                "TRVLUTADR "},
    {SHDISP_CLMR_CUST_TRVCTL,                   "TRVCTL    "},
    {SHDISP_CLMR_CUST_TRVCFG,                   "TRVCFG    "},
    {SHDISP_CLMR_CUST_TRVTEXOFT,                "TRVTEXOFT "},
    {SHDISP_CLMR_CUST_TRVPAT0,                  "TRVPAT0   "},
    {SHDISP_CLMR_CUST_TRVPAT1,                  "TRVPAT1   "},
    {SHDISP_CLMR_CUST_TRVFPAT0,                 "TRVFPAT0  "},
    {SHDISP_CLMR_CUST_TRVFPAT1,                 "TRVFPAT1  "},
    {SHDISP_CLMR_CUST_TRVCOLMAP0,               "TRVCOLMAP0"},
    {SHDISP_CLMR_CUST_TRVCOLMAP1,               "TRVCOLMAP1"},
    {SHDISP_CLMR_CUST_TRVSHIFT,                 "TRVSHIFT  "},
    {SHDISP_CLMR_CUST_TRVANGLX,                 "TRVANGLX  "},
    {SHDISP_CLMR_CUST_TRVANGLY,                 "TRVANGLY  "},
    {SHDISP_CLMR_CUST_TRVANGLH,                 "TRVANGLH  "},
    {SHDISP_CLMR_CUST_TRVOSTFLT0,               "TRVOSTFLT0"},
    {SHDISP_CLMR_CUST_TRVOSTFLT1,               "TRVOSTFLT1"},
    {SHDISP_CLMR_CUST_TRVCSCREF,                "TRVCSCREF "},
    {SHDISP_CLMR_CUST_TRVADPPAL,                "TRVADPPAL "},
    {SHDISP_CLMR_CUST_TRVSTEP,                  "TRVSTEP   "},
    {SHDISP_CLMR_CUST_TRVADPSET,                "TRVADPSET "},
    {SHDISP_CLMR_CUST_TRVEX,                    "TRVEX     "},
    {SHDISP_CLMR_CUST_TRVREQ,                   "TRVREQ    "},
    {SHDISP_CLMR_CUST_TRVRAMHW,                 "TRVRAMHW  "},
    { 0x8378,                                   "          "},
    { 0x837C,                                   "          "},
    { 0x8380,                                   "          "},
    { 0x8384,                                   "          "},
    { 0x8388,                                   "          "},
    { 0x838C,                                   "          "},
    { 0x8390,                                   "          "},
    { 0x8394,                                   "          "},
    { 0x8398,                                   "          "},
    { 0x839C,                                   "          "},
    { 0x83A0,                                   "          "},
    { 0x83A4,                                   "          "},
    { 0x83A8,                                   "          "},
    { 0x83AC,                                   "          "},
    { 0x83B0,                                   "          "},
    { 0x83B4,                                   "          "},
    { 0x83B8,                                   "          "},
    { 0x83BC,                                   "          "},
    { 0x83C0,                                   "          "},
    { 0x83C4,                                   "          "},
    { 0x83C8,                                   "          "},
    { 0x83CC,                                   "          "},
    { 0x83D0,                                   "          "},
    { 0x83D4,                                   "          "},
    { 0x83D8,                                   "          "},
    { 0x83DC,                                   "          "},
    { 0x83E0,                                   "          "},
    { 0x83E4,                                   "          "},
    { 0x83E8,                                   "          "},
    { 0x83EC,                                   "          "},
    { 0x83F0,                                   "          "},
    { 0x83F4,                                   "          "},
    { 0x83F8,                                   "          "},
    { 0x83FC,                                   "          "},

    {SHDISP_CLMR_CUST_SVCTASAXU,                "SVCTASAXU "},
    {SHDISP_CLMR_CUST_SVCTASAYU,                "SVCTASAYU "},
    {SHDISP_CLMR_CUST_SVCTAWSU,                 "SVCTAWSU  "},
    {SHDISP_CLMR_CUST_SVCTAHSU,                 "SVCTAHSU  "},
    {SHDISP_CLMR_CUST_SVCTASAXL,                "SVCTASAXL "},
    {SHDISP_CLMR_CUST_SVCTASAYL,                "SVCTASAYL "},
    {SHDISP_CLMR_CUST_SVCTAWSL,                 "SVCTAWSL  "},
    {SHDISP_CLMR_CUST_SVCTAHSL,                 "SVCTAHSL  "},
    { 0x8420,                                   "          "},
    { 0x8424,                                   "          "},
    { 0x8428,                                   "          "},
    { 0x842C,                                   "          "},
    { 0x8430,                                   "          "},
    { 0x8434,                                   "          "},
    { 0x8438,                                   "          "},
    { 0x843C,                                   "          "},
    {SHDISP_CLMR_CUST_SVCTYCCRSU,               "SVCTYCCRSU"},
    {SHDISP_CLMR_CUST_SVCTYCBLSU,               "SVCTYCBLSU"},
    {SHDISP_CLMR_CUST_SVCTYCBRSU,               "SVCTYCBRSU"},
    {SHDISP_CLMR_CUST_SVCTYCMPAR0U,             "SVCTYCMPAR0U"},
    {SHDISP_CLMR_CUST_SVCTYCMPAR1U,             "SVCTYCMPAR1U"},
    {SHDISP_CLMR_CUST_SVCTYCSFSU,               "SVCTYCSFSU"},
    {SHDISP_CLMR_CUST_SVCTYCUVCSU,              "SVCTYCUVCSU"},
    {SHDISP_CLMR_CUST_SVCTYCDEGU,               "SVCTYCDEGU"},
    {SHDISP_CLMR_CUST_SVCTYCRGBA1U,             "SVCTYCRGBA1U"},
    {SHDISP_CLMR_CUST_SVCTYCCMYA1U,             "SVCTYCCMYA1U"},
    {SHDISP_CLMR_CUST_SVCTYCRGBA2U,             "SVCTYCRGBA2U"},
    {SHDISP_CLMR_CUST_SVCTYCCMYA2U,             "SVCTYCCMYA2U"},
    {SHDISP_CLMR_CUST_SVCTYCRGBA3U,             "SVCTYCRGBA3U"},
    {SHDISP_CLMR_CUST_SVCTYCCMYA3U,             "SVCTYCCMYA3U"},
    {SHDISP_CLMR_CUST_SVCTYCMCSU,               "SVCTYCMCSU"},
    { 0x847C,                                   "          "},
    {SHDISP_CLMR_CUST_SVCTYCCRSL,               "SVCTYCCRSL"},
    {SHDISP_CLMR_CUST_SVCTYCBLSL,               "SVCTYCBLSL"},
    {SHDISP_CLMR_CUST_SVCTYCBRSL,               "SVCTYCBRSL"},
    {SHDISP_CLMR_CUST_SVCTYCMPAR0L,             "SVCTYCMPAR0L"},
    {SHDISP_CLMR_CUST_SVCTYCMPAR1L,             "SVCTYCMPAR1L"},
    {SHDISP_CLMR_CUST_SVCTYCSFSL,               "SVCTYCSFSL"},
    {SHDISP_CLMR_CUST_SVCTYCUVCSL,              "SVCTYCUVCSL"},
    {SHDISP_CLMR_CUST_SVCTYCDEGL,               "SVCTYCDEGL"},
    {SHDISP_CLMR_CUST_SVCTYCRGBA1L,             "SVCTYCRGBA1L"},
    {SHDISP_CLMR_CUST_SVCTYCCMYA1L,             "SVCTYCCMYA1L"},
    {SHDISP_CLMR_CUST_SVCTYCRGBA2L,             "SVCTYCRGBA2L"},
    {SHDISP_CLMR_CUST_SVCTYCCMYA2L,             "SVCTYCCMYA2L"},
    {SHDISP_CLMR_CUST_SVCTYCRGBA3L,             "SVCTYCRGBA3L"},
    {SHDISP_CLMR_CUST_SVCTYCCMYA3L,             "SVCTYCCMYA3L"},
    {SHDISP_CLMR_CUST_SVCTYCMCSL,               "SVCTYCMCSL"},
    { 0x84BC,                                   "          "},
    { 0x84C0,                                   "          "},
    { 0x84C4,                                   "          "},
    { 0x84C8,                                   "          "},
    { 0x84CC,                                   "          "},
    { 0x84D0,                                   "          "},
    { 0x84D4,                                   "          "},
    { 0x84D8,                                   "          "},
    { 0x84DC,                                   "          "},
    { 0x84E0,                                   "          "},
    { 0x84E4,                                   "          "},
    { 0x84E8,                                   "          "},
    { 0x84EC,                                   "          "},
    { 0x84F0,                                   "          "},
    { 0x84F4,                                   "          "},
    { 0x84F8,                                   "          "},
    { 0x84FC,                                   "          "},

    {SHDISP_CLMR_CUST_PCAPRM0,                  "PCAPRM0   "},
    {SHDISP_CLMR_CUST_PCAPRM1,                  "PCAPRM1   "},
    {SHDISP_CLMR_CUST_PCAPRM2,                  "PCAPRM2   "},
    {SHDISP_CLMR_CUST_PCAPRM3,                  "PCAPRM3   "},
    {SHDISP_CLMR_CUST_PCAPRM4,                  "PCAPRM4   "},
    {SHDISP_CLMR_CUST_PCAPRM5,                  "PCAPRM5   "},
    {SHDISP_CLMR_CUST_PCAPRM6,                  "PCAPRM6   "},
    {SHDISP_CLMR_CUST_PCAPRM7,                  "PCAPRM7   "},
    {SHDISP_CLMR_CUST_PCAPRM8,                  "PCAPRM8   "},
    {SHDISP_CLMR_CUST_PCAPRM9,                  "PCAPRM9   "},
    {SHDISP_CLMR_CUST_PCAPRM10,                 "PCAPRM10  "},
    {SHDISP_CLMR_CUST_PCAPRM11,                 "PCAPRM11  "},
    {SHDISP_CLMR_CUST_PCAPRM12,                 "PCAPRM12  "},
    {SHDISP_CLMR_CUST_PCAPRM13,                 "PCAPRM13  "},
    {SHDISP_CLMR_CUST_PCAPRM14,                 "PCAPRM14  "},
    {SHDISP_CLMR_CUST_PCAPRM15,                 "PCAPRM15  "},
    {SHDISP_CLMR_CUST_PCAPRM16,                 "PCAPRM16  "},
    {SHDISP_CLMR_CUST_PCAPRM17,                 "PCAPRM17  "},
    {SHDISP_CLMR_CUST_PCAPRM18,                 "PCAPRM18  "},
    { 0x854C,                                   "          "},
    { 0x8550,                                   "          "},
    { 0x8554,                                   "          "},
    { 0x8558,                                   "          "},
    { 0x855C,                                   "          "},
    { 0x8560,                                   "          "},
    { 0x8564,                                   "          "},
    { 0x8568,                                   "          "},
    { 0x856C,                                   "          "},
    { 0x8570,                                   "          "},
    { 0x8574,                                   "          "},
    { 0x8578,                                   "          "},
    { 0x857C,                                   "          "},
    { 0x8580,                                   "          "},
    { 0x8584,                                   "          "},
    { 0x8588,                                   "          "},
    { 0x858C,                                   "          "},
    { 0x8590,                                   "          "},
    { 0x8594,                                   "          "},
    { 0x8598,                                   "          "},
    { 0x859C,                                   "          "},
    {SHDISP_CLMR_CUST_HSV0,                     "HSV0      "},
    {SHDISP_CLMR_CUST_HSV1,                     "HSV1      "},
    {SHDISP_CLMR_CUST_HSV2,                     "HSV2      "},
    {SHDISP_CLMR_CUST_HSV3,                     "HSV3      "},
    {SHDISP_CLMR_CUST_HSV4,                     "HSV4      "},
    {SHDISP_CLMR_CUST_HSV5,                     "HSV5      "},
    {SHDISP_CLMR_CUST_HSV6,                     "HSV6      "},
    {SHDISP_CLMR_CUST_HSV7,                     "HSV7      "},
    {SHDISP_CLMR_CUST_HSV8,                     "HSV8      "},
    {SHDISP_CLMR_CUST_HSV9,                     "HSV9      "},
    {SHDISP_CLMR_CUST_HSV10,                    "HSV10     "},
    {SHDISP_CLMR_CUST_HSV11,                    "HSV11     "},
    {SHDISP_CLMR_CUST_HSV12,                    "HSV12     "},
    {SHDISP_CLMR_CUST_HSV13,                    "HSV13     "},
    {SHDISP_CLMR_CUST_HSV14,                    "HSV14     "},
    {SHDISP_CLMR_CUST_EACT0,                    "EACT0     "},
    {SHDISP_CLMR_CUST_EACT1,                    "EACT1     "},
    {SHDISP_CLMR_CUST_EACT2,                    "EACT2     "},
    {SHDISP_CLMR_CUST_EACT3,                    "EACT3     "},
    {SHDISP_CLMR_CUST_EACT4,                    "EACT4     "},
    { 0x85F0,                                   "          "},
    { 0x85F4,                                   "          "},
    { 0x85F8,                                   "          "},
    { 0x85FC,                                   "          "},

    {SHDISP_CLMR_CUST_SVCTDSP1U,                "SVCTDSP1U "},
    {SHDISP_CLMR_CUST_SVCTDSP2U,                "SVCTDSP2U "},
    {SHDISP_CLMR_CUST_SVCTDSP3U,                "SVCTDSP3U "},
    {SHDISP_CLMR_CUST_SVCTDSP4U,                "SVCTDSP4U "},
    {SHDISP_CLMR_CUST_SVCTDSPHU,                "SVCTDSPHU "},
    {SHDISP_CLMR_CUST_SVCTDHUEU,                "SVCTDHUEU "},
    {SHDISP_CLMR_CUST_SVCTDAXISU,               "SVCTDAXISU"},
    {SHDISP_CLMR_CUST_SVCTDUNITU,               "SVCTDUNITU"},
    {SHDISP_CLMR_CUST_SVCTDSP1L,                "SVCTDSP1L "},
    {SHDISP_CLMR_CUST_SVCTDSP2L,                "SVCTDSP2L "},
    {SHDISP_CLMR_CUST_SVCTDSP3L,                "SVCTDSP3L "},
    {SHDISP_CLMR_CUST_SVCTDSP4L,                "SVCTDSP4L "},
    {SHDISP_CLMR_CUST_SVCTDSPHL,                "SVCTDSPHL "},
    {SHDISP_CLMR_CUST_SVCTDHUEL,                "SVCTDHUEL "},
    {SHDISP_CLMR_CUST_SVCTDAXISL,               "SVCTDAXISL"},
    {SHDISP_CLMR_CUST_SVCTDUNITL,               "SVCTDUNITL"},
    {SHDISP_CLMR_CUST_SVCTRCCSRU,               "SVCTRCCSRU"},
    {SHDISP_CLMR_CUST_SVCTRCCSGU,               "SVCTRCCSGU"},
    {SHDISP_CLMR_CUST_SVCTRCCSBU,               "SVCTRCCSBU"},
    {SHDISP_CLMR_CUST_SVCTRCCRVU,               "SVCTRCCRVU"},
    {SHDISP_CLMR_CUST_SVCTRCCSRL,               "SVCTRCCSRL"},
    {SHDISP_CLMR_CUST_SVCTRCCSGL,               "SVCTRCCSGL"},
    {SHDISP_CLMR_CUST_SVCTRCCSBL,               "SVCTRCCSBL"},
    {SHDISP_CLMR_CUST_SVCTRCCRVL,               "SVCTRCCRVL"},
    { 0x8660,                                   "          "},
    { 0x8664,                                   "          "},
    { 0x8668,                                   "          "},
    { 0x866C,                                   "          "},
    { 0x8670,                                   "          "},
    { 0x8674,                                   "          "},
    { 0x8678,                                   "          "},
    { 0x867C,                                   "          "},
    {SHDISP_CLMR_CUST_SVCTYBBLRU,               "SVCTYBBLRU"},
    {SHDISP_CLMR_CUST_SVCTYBBRRU,               "SVCTYBBRRU"},
    {SHDISP_CLMR_CUST_SVCTYBSCRU,               "SVCTYBSCRU"},
    {SHDISP_CLMR_CUST_SVCTYBCRVU,               "SVCTYBCRVU"},
    {SHDISP_CLMR_CUST_SVCTYBBLRL,               "SVCTYBBLRL"},
    {SHDISP_CLMR_CUST_SVCTYBBRRL,               "SVCTYBBRRL"},
    {SHDISP_CLMR_CUST_SVCTYBSCRL,               "SVCTYBSCRL"},
    {SHDISP_CLMR_CUST_SVCTYBCRVL,               "SVCTYBCRVL"},
    {SHDISP_CLMR_CUST_SVCTYBBLRS0,              "SVCTYBBLRS0"},
    {SHDISP_CLMR_CUST_SVCTYBBLRS1,              "SVCTYBBLRS1"},
    {SHDISP_CLMR_CUST_SVCTYBBLRS2,              "SVCTYBBLRS2"},
    {SHDISP_CLMR_CUST_SVCTYBBLRS3,              "SVCTYBBLRS3"},
    {SHDISP_CLMR_CUST_SVCTYBBLRS4,              "SVCTYBBLRS4"},
    {SHDISP_CLMR_CUST_SVCTYBBLRS5,              "SVCTYBBLRS5"},
    {SHDISP_CLMR_CUST_SVCTYBBLRS6,              "SVCTYBBLRS6"},
    {SHDISP_CLMR_CUST_SVCTYBBLRS7,              "SVCTYBBLRS7"},
    {SHDISP_CLMR_CUST_SVCTYBBRRS0,              "SVCTYBBRRS0"},
    {SHDISP_CLMR_CUST_SVCTYBBRRS1,              "SVCTYBBRRS1"},
    {SHDISP_CLMR_CUST_SVCTYBBRRS2,              "SVCTYBBRRS2"},
    {SHDISP_CLMR_CUST_SVCTYBBRRS3,              "SVCTYBBRRS3"},
    {SHDISP_CLMR_CUST_SVCTYBBRRS4,              "SVCTYBBRRS4"},
    {SHDISP_CLMR_CUST_SVCTYBBRRS5,              "SVCTYBBRRS5"},
    {SHDISP_CLMR_CUST_SVCTYBBRRS6,              "SVCTYBBRRS6"},
    {SHDISP_CLMR_CUST_SVCTYBBRRS7,              "SVCTYBBRRS7"},
    {SHDISP_CLMR_CUST_SVCTYBSCRS0,              "SVCTYBSCRS0"},
    {SHDISP_CLMR_CUST_SVCTYBSCRS1,              "SVCTYBSCRS1"},
    {SHDISP_CLMR_CUST_SVCTYBSCRS2,              "SVCTYBSCRS2"},
    {SHDISP_CLMR_CUST_SVCTYBSCRS3,              "SVCTYBSCRS3"},
    {SHDISP_CLMR_CUST_SVCTYBSCRS4,              "SVCTYBSCRS4"},
    {SHDISP_CLMR_CUST_SVCTYBSCRS5,              "SVCTYBSCRS5"},
    {SHDISP_CLMR_CUST_SVCTYBSCRS6,              "SVCTYBSCRS6"},
    {SHDISP_CLMR_CUST_SVCTYBSCRS7,              "SVCTYBSCRS7"},

    { 0x8700,                                   "          "},
    { 0x8704,                                   "          "},
    { 0x8708,                                   "          "},
    { 0x870C,                                   "          "},
    { 0x8710,                                   "          "},
    { 0x8714,                                   "          "},
    { 0x8718,                                   "          "},
    { 0x871C,                                   "          "},
    { 0x8720,                                   "          "},
    { 0x8724,                                   "          "},
    {SHDISP_CLMR_CUST_SMITHSYNC,                "SMITHSYNC "},
    { 0x872C,                                   "          "},
    {SHDISP_CLMR_CUST_SVINTMASK0,               "SVINTMASK0"},
    { 0x8734,                                   "          "},
    { 0x8738,                                   "          "},
    { 0x873C,                                   "          "},
    {SHDISP_CLMR_CUST_HISTINTF,                 "HISTINTF  "},
    {SHDISP_CLMR_CUST_HISTINTE,                 "HISTINTE  "},
    {SHDISP_CLMR_CUST_HISTINTW,                 "HISTINTW  "},
    { 0x874C,                                   "          "},
    {SHDISP_CLMR_CUST_HISTCTRL,                 "HISTCTRL  "},
    {SHDISP_CLMR_CUST_HISTMODE,                 "HISTMODE  "},
    {SHDISP_CLMR_CUST_HISTAREAX,                "HISTAREAX "},
    {SHDISP_CLMR_CUST_HISTAREAY,                "HISTAREAY "},
    {SHDISP_CLMR_CUST_HISTAREAW,                "HISTAREAW "},
    {SHDISP_CLMR_CUST_HISTAREAH,                "HISTAREAH "},
    {SHDISP_CLMR_CUST_HISTBLEND,                "HISTBLEND "},
    { 0x876C,                                   "          "},
    { 0x8770,                                   "          "},
    { 0x8774,                                   "          "},
    { 0x8778,                                   "          "},
    { 0x877C,                                   "          "},
    {SHDISP_CLMR_CUST_HISTDAT0,                 "HISTDAT0  "},
    {SHDISP_CLMR_CUST_HISTDAT1,                 "HISTDAT1  "},
    {SHDISP_CLMR_CUST_HISTDAT2,                 "HISTDAT2  "},
    {SHDISP_CLMR_CUST_HISTDAT3,                 "HISTDAT3  "},
    {SHDISP_CLMR_CUST_HISTDAT4,                 "HISTDAT4  "},
    {SHDISP_CLMR_CUST_HISTDAT5,                 "HISTDAT5  "},
    {SHDISP_CLMR_CUST_HISTDAT6,                 "HISTDAT6  "},
    {SHDISP_CLMR_CUST_HISTDAT7,                 "HISTDAT7  "},
    {SHDISP_CLMR_CUST_HISTDAT8,                 "HISTDAT8  "},
    {SHDISP_CLMR_CUST_HISTDAT9,                 "HISTDAT9  "},
    {SHDISP_CLMR_CUST_HISTDAT10,                "HISTDAT10 "},
    {SHDISP_CLMR_CUST_HISTDAT11,                "HISTDAT11 "},
    {SHDISP_CLMR_CUST_HISTDAT12,                "HISTDAT12 "},
    {SHDISP_CLMR_CUST_HISTDAT13,                "HISTDAT13 "},
    {SHDISP_CLMR_CUST_HISTDAT14,                "HISTDAT14 "},
    {SHDISP_CLMR_CUST_HISTDAT15,                "HISTDAT15 "},
    {SHDISP_CLMR_CUST_HISTDAT16,                "HISTDAT16 "},
    {SHDISP_CLMR_CUST_HISTDAT17,                "HISTDAT17 "},
    {SHDISP_CLMR_CUST_HISTDAT18,                "HISTDAT18 "},
    {SHDISP_CLMR_CUST_HISTDAT19,                "HISTDAT19 "},
    {SHDISP_CLMR_CUST_HISTDAT20,                "HISTDAT20 "},
    {SHDISP_CLMR_CUST_HISTDAT21,                "HISTDAT21 "},
    {SHDISP_CLMR_CUST_HISTDAT22,                "HISTDAT22 "},
    {SHDISP_CLMR_CUST_HISTDAT23,                "HISTDAT23 "},
    {SHDISP_CLMR_CUST_HISTDAT24,                "HISTDAT24 "},
    {SHDISP_CLMR_CUST_HISTDAT25,                "HISTDAT25 "},
    {SHDISP_CLMR_CUST_HISTDAT26,                "HISTDAT26 "},
    {SHDISP_CLMR_CUST_HISTDAT27,                "HISTDAT27 "},
    {SHDISP_CLMR_CUST_HISTDAT28,                "HISTDAT28 "},
    {SHDISP_CLMR_CUST_HISTDAT29,                "HISTDAT29 "},
    {SHDISP_CLMR_CUST_HISTDAT30,                "HISTDAT30 "},
    {SHDISP_CLMR_CUST_HISTDAT31,                "HISTDAT31 "},
};

#if defined(USE_LINUX) || (defined(SHDISP_CLMR_FW_TIMEOUT_DUMP) && defined(SHDISP_DBG_BOOTLOG_REGDUMP))
/* ------------------------------------------------------------------------- */
/*      shdisp_clmr_reg_dump                                                 */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_reg_dump(unsigned short reg)
{
    int ret;
    int size = sizeof(char) * 4;
    unsigned long regVal;
    union data_t {
        unsigned long lDat;
        unsigned char cDat[4];
    } sData;

    shdisp_SYS_clmr_sio_transfer(reg, NULL, 0, &sData.cDat[0], size);

    ret = SHDISP_RESULT_SUCCESS;
    regVal = ntohl(sData.lDat);
    SHDISP_ERR("[%d] dump.reg=0x%x val=0x%08x\n", __LINE__, reg, (int)regVal);

    return ret;
}
#endif /* defined(USE_LINUX) || (defined(SHDISP_CLMR_FW_TIMEOUT_DUMP) && defined(SHDISP_DBG_BOOTLOG_REGDUMP)) */

/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_api_RegDump                                              */
/*---------------------------------------------------------------------------*/
void shdisp_clmr_api_RegDump(int type)
{
    char cstm_sel = 0;
    char pclkon   = 0;
    int i = 0;
    int size = 0;
    int bufSize = sizeof(char) * 4;
    unsigned short addr = 0;
    unsigned char reg[4];

    SHDISP_TRACE("in\n");

    switch (type) {
    case CALI_REG_DUMP:
    case CALI_ALL_DUMP:
        size = ARRAY_SIZE(clmr_reg);
        for (i = 0; i < size; i++) {
            memset(reg, 0, sizeof(reg));
            addr = (unsigned short)clmr_reg[i].addr;
            shdisp_SYS_clmr_sio_transfer(addr, NULL, 0, &reg[0], bufSize);
            printk(KERN_INFO "0x%04X(%s)        =0x%02X%02X%02X%02X",
                                addr, (unsigned char *)clmr_reg[i].regName,
                                    reg[0], reg[1], reg[2], reg[3]);
        }
        if (CALI_REG_DUMP == type) {
            break;
        }

    case CALI_CUST_DUMP:
        memset(reg, 0, bufSize);
        shdisp_SYS_clmr_sio_transfer(
            SHDISP_CLMR_REG_SYSCTL, NULL, 0, &reg[0], bufSize);
        if ((reg[3] & 0x01) != 0x01) {
            cstm_sel = 1;
            reg[3] |= 0x01;
            shdisp_SYS_clmr_sio_transfer(
                SHDISP_CLMR_REG_SYSCTL, &reg[0], bufSize, NULL, 0);
        }

        memset(reg, 0, bufSize);
        shdisp_SYS_clmr_sio_transfer(
            SHDISP_CLMR_REG_CSTMSYS, NULL, 0, &reg[0], bufSize);
        if ((reg[3] & 0x01) != 0x01) {
            pclkon = 1;
            reg[3] |= 0x01;
            shdisp_SYS_clmr_sio_transfer(
                SHDISP_CLMR_REG_CSTMSYS, &reg[0], bufSize, NULL, 0);
        }


        size = ARRAY_SIZE(clmr_customReg);
        for (i = 0; i < size; i++) {
            memset(reg, 0, bufSize);
            addr = (unsigned short)clmr_customReg[i].addr;
            shdisp_SYS_clmr_sio_transfer(addr, NULL, 0, &reg[0], bufSize);
            printk(KERN_INFO "0x%04X(%s)        =0x%02X%02X%02X%02X",
                        addr, (unsigned char *)clmr_customReg[i].regName,
                                reg[0], reg[1], reg[2], reg[3]);
        }


        if (cstm_sel == 1) {
            memset(reg, 0, bufSize);
            shdisp_SYS_clmr_sio_transfer(
                SHDISP_CLMR_REG_SYSCTL, NULL, 0, &reg[0], bufSize);
            reg[3] &= ~0x01;
            shdisp_SYS_clmr_sio_transfer(
                SHDISP_CLMR_REG_SYSCTL, &reg[0], bufSize, NULL, 0);
            cstm_sel = 0;
        }

        if (pclkon == 1) {
            memset(reg, 0, bufSize);
            shdisp_SYS_clmr_sio_transfer(
                SHDISP_CLMR_REG_CSTMSYS, NULL, 0, &reg[0], bufSize);
            reg[3] &= ~0x01;
            shdisp_SYS_clmr_sio_transfer(
                SHDISP_CLMR_REG_CSTMSYS, &reg[0], bufSize, NULL, 0);
            pclkon = 0;
        }
        break;
    case CALI_FW_DUMP:
        printk(KERN_INFO "[SHDISP] arm_fw_size        = %04ld", gArm_fw_size);
        break;
    case CALI_VER_DUMP:
        memset(reg, 0, sizeof(reg));
        shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_REG_TESTMODE14, NULL, 0, reg, bufSize);
        printk(KERN_INFO "[SHDISP] FW Ver:%d.%d%d%d\n", ((reg[0] >> 4) & 0x0f), (reg[0] & 0x0f),
                                                        ((reg[1] >> 4) & 0x0f), (reg[1] & 0x0f));
        break;
    default:
        SHDISP_ERR("error. type=%d\n", type);
        break;
    }

    SHDISP_TRACE("out\n");
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_boot_start                                                    */
/* ------------------------------------------------------------------------- */
static void shdisp_clmr_boot_start(void)
{
    struct shdisp_clmr_ctrl_t *clmr_ctrl;
    clmr_ctrl = &shdisp_clmr_ctrl;

    INIT_COMPLETION(shdisp_clmr_ctrl.fw_boot_comp);
    down(&clmr_ctrl->fw_boot_sem);
    clmr_ctrl->fw_boot_excute = 1;
    clmr_ctrl->fw_cmd_excute = 0;
    clmr_ctrl->eDramPtr_rst_excute = 0;
    up(&clmr_ctrl->fw_boot_sem);
}


/* ------------------------------------------------------------------------- */
/* shdisp_clmr_cmd_start                                                     */
/* ------------------------------------------------------------------------- */
static void shdisp_clmr_cmd_start(void)
{
    struct shdisp_clmr_ctrl_t *clmr_ctrl;
    clmr_ctrl = &shdisp_clmr_ctrl;

    INIT_COMPLETION(shdisp_clmr_ctrl.fw_cmd_comp);
    down(&clmr_ctrl->fw_cmd_sem);
    clmr_ctrl->fw_cmd_excute = 1;
    up(&clmr_ctrl->fw_cmd_sem);
}


/* ------------------------------------------------------------------------- */
/* shdisp_clmr_eDramPtr_rst_start                                            */
/* ------------------------------------------------------------------------- */
static void shdisp_clmr_eDramPtr_rst_start(void)
{
    struct shdisp_clmr_ctrl_t *clmr_ctrl;
    clmr_ctrl = &shdisp_clmr_ctrl;

    INIT_COMPLETION(shdisp_clmr_ctrl.eDramPtr_rst_comp);
    down(&clmr_ctrl->eDramPtr_rst_sem);
    clmr_ctrl->eDramPtr_rst_excute = 1;
    up(&clmr_ctrl->eDramPtr_rst_sem);
}


#ifdef SHDISP_CLMR_FW_TIMEOUT_DUMP
/*---------------------------------------------------------------------------*/
/*      shdisp_clmr_fw_timeout_RegDump                                       */
/*---------------------------------------------------------------------------*/
static int shdisp_clmr_fw_timeout_RegDump(struct shdisp_dbg_ptrinfo *ring_dump_buf,
                                          struct shdisp_dbg_ptrinfo *stack_dump_buf)
{
    struct shdisp_dbg_ptrinfo edram_dump_buf = {0};
    struct shdisp_dbg_ptrinfo sram_dump_buf = {0};
    int error = -EINVAL;

    SHDISP_INFO("in\n");

    edram_dump_buf.ptr = kzalloc(SHDISP_CALI_EDRAM_DUMP_SIZE, GFP_KERNEL);
    if (edram_dump_buf.ptr == NULL) {
        SHDISP_ERR("allocate edram dump buffer error. [no memory]\n");
        error = -ENOMEM;
        goto errout;
    } else {
        edram_dump_buf.length = SHDISP_CALI_EDRAM_DUMP_SIZE;
        edram_dump_buf.need_free = 1;
    }

    sram_dump_buf.ptr = kzalloc(SHDISP_CALI_SRAM_DUMP_SIZE, GFP_KERNEL);
    if (sram_dump_buf.ptr == NULL) {
        SHDISP_ERR("allocate sram dump buffer error. [no memory]\n");
        error = -ENOMEM;
        goto errout;
    } else {
        sram_dump_buf.length = SHDISP_CALI_SRAM_DUMP_SIZE;
        sram_dump_buf.need_free = 1;
    }

    shdisp_clmr_ram_dump_begin();
    shdisp_clmr_edram_dump(edram_dump_buf.ptr, edram_dump_buf.length);
    shdisp_clmr_sram_dump(sram_dump_buf.ptr, sram_dump_buf.length);
    shdisp_clmr_ram_dump_end();

    error = shdisp_dbg_api_display_dump(
            SHDISP_DBG_MODE_LINUX,
            ring_dump_buf,
            stack_dump_buf,
            &edram_dump_buf,
            &sram_dump_buf);
    if (error != 0) {
        SHDISP_ERR("shdisp_dbg_api_display_dump() failed.\n");
        goto errout;
    }

    SHDISP_INFO("out normaly finished.\n");
    return SHDISP_RESULT_SUCCESS;

errout:
    if (edram_dump_buf.ptr != NULL) {
        kfree(edram_dump_buf.ptr);
    }
    if (sram_dump_buf.ptr != NULL) {
        kfree(sram_dump_buf.ptr);
    }

    SHDISP_ERR("abnormaly finished. error=%d\n", error);
    return error;
}
#endif /* SHDISP_CLMR_FW_TIMEOUT_DUMP */

#ifdef SHDISP_CLMR_FW_TIMEOUT_DUMP
/* ------------------------------------------------------------------------- */
/* shdisp_clmr_ram_dump_begin                                                */
/* ------------------------------------------------------------------------- */
static void shdisp_clmr_ram_dump_begin(void)
{
    int count = 0;
    int size = 0;

    size = ARRAY_SIZE(eDRAM_dump_setting_FW_stop);
    for (count = 0; count < size; count++) {
        shdisp_clmr_regSet(&eDRAM_dump_setting_FW_stop[count]);
    }
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_edram_dump                                                    */
/* ------------------------------------------------------------------------- */
static size_t shdisp_clmr_edram_dump(unsigned char *edram_dump_buf, size_t length)
{
    int count = 0;
    int size = 0;

    if (length < SHDISP_CALI_EDRAM_DUMP_SIZE) {
        SHDISP_ERR("buffer size too small.\n");
        return 0;
    }

    size = ARRAY_SIZE(eDRAM_dump_setting_1st);
    for (count = 0; count < size; count++) {
        shdisp_clmr_regSet(&eDRAM_dump_setting_1st[count]);
    }

    shdisp_SYS_clmr_sio_eDram_transfer( SHDISP_CLMR_EDRAM_C9, edram_dump_buf, (SHDISP_CALI_EDRAM_DUMP_SIZE / 2) );

    edram_dump_buf = edram_dump_buf + (SHDISP_CALI_EDRAM_DUMP_SIZE / 2);

    size = ARRAY_SIZE(eDRAM_dump_setting_2nd);
    for (count = 0; count < size; count++) {
        shdisp_clmr_regSet(&eDRAM_dump_setting_2nd[count]);
    }

    shdisp_SYS_clmr_sio_eDram_transfer( SHDISP_CLMR_EDRAM_C9, edram_dump_buf, (SHDISP_CALI_EDRAM_DUMP_SIZE / 2) );

    return SHDISP_CALI_EDRAM_DUMP_SIZE;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sram_dump                                                     */
/* ------------------------------------------------------------------------- */
static size_t shdisp_clmr_sram_dump(unsigned char *sram_dump_buf, size_t length)
{
    int count = 0;
    int size = 0;
    int i = 0;

    if (length < SHDISP_CALI_SRAM_DUMP_SIZE) {
        SHDISP_ERR("buffer size too small.\n");
        return 0;
    }

    size = ARRAY_SIZE(SRAM_dump_access_setting);
    for (count = 0; count < size; count++) {
        shdisp_clmr_regSet(&SRAM_dump_access_setting[count]);
    }

    for (i = 0; i < SHDISP_CALI_SRAM_DUMP_COUNT; i++) {
        shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_REG_ARMSRAMDAT, NULL, 0, sram_dump_buf, 4);

        sram_dump_buf += 4;
    }

    return SHDISP_CALI_SRAM_DUMP_SIZE;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_ram_dump_end                                                  */
/* ------------------------------------------------------------------------- */
static void shdisp_clmr_ram_dump_end(void)
{
    int count = 0;
    int size = 0;

    size = ARRAY_SIZE(SRAM_dump_arm_setting);
    for (count = 0; count < size; count++) {
        shdisp_clmr_regSet(&SRAM_dump_arm_setting[count]);
    }
}
#endif /* SHDISP_CLMR_FW_TIMEOUT_DUMP */

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_set_fw_chg_flg                                                */
/* ------------------------------------------------------------------------- */
void shdisp_clmr_set_fw_chg_flg(unsigned int flg)
{
    struct file *fp;
    unsigned int rdlen;
    unsigned long fw_size = 0;
    unsigned char fwHeader[16];
    unsigned char * heap = NULL;

    if (gArm_fw_chg_flg) {
        kfree(gArm_fw);
        gArm_fw = arm_fw;
        gArm_fw_size = arm_fw_size;
        gArm_fw_base = arm_fw_base;
        gArm_fw_chg_flg = 0;
    }

    if (!flg) {
        SHDISP_DEBUG("FW dynachange stop.\n");
        return;
    }


    fp = filp_open("/data/shdisp_fw.bin", O_RDONLY, 0);
    if (IS_ERR(fp)) {
        SHDISP_ERR("FW file nothing!!\n");
        return;
    }

    rdlen = kernel_read(fp, fp->f_pos, fwHeader, 16);

    if (rdlen < 16) {
        SHDISP_ERR("FW file header read error!!\n");
        filp_close(fp, NULL);
        return;
    }

    fw_size = ((unsigned long)fwHeader[0] + (unsigned long)(fwHeader[1] << 8));
    heap = kmalloc(fw_size * 16, GFP_KERNEL | __GFP_DMA);

    if (!heap) {
        SHDISP_ERR("fw_buf alloc error!!\n");
        filp_close(fp, NULL);
        return;
    }

    fp->f_pos += rdlen;
    rdlen = kernel_read(fp, fp->f_pos, heap, fw_size * 16);

    if (rdlen < fw_size * 16) {
        SHDISP_ERR("fw load error!!\n");
        kfree(heap);
        filp_close(fp, NULL);
        return;
    }

    gArm_fw = heap;
    gArm_fw_size = fw_size;
    gArm_fw_base = ((unsigned short)fwHeader[2] + (unsigned short)(fwHeader[3] << 8));
    gArm_fw_chg_flg = flg;

    filp_close(fp, NULL);
    SHDISP_DEBUG("FW dynachange fwload success size= %lu * 16.\n", gArm_fw_size);
}
MODULE_DESCRIPTION("SHARP DISPLAY DRIVER MODULE");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("1.00");

#ifdef KERNEL_CALL_PIC_ADJ_MDP
/* ------------------------------------------------------------------------- */
/* shdisp_clmr_set_pic_adj_data                                              */
/* ------------------------------------------------------------------------- */
static int shdisp_clmr_set_pic_adj_data(int mode, unsigned short ap_type)
{
    int ret = SHDISP_RESULT_SUCCESS;
    struct msmfb_pipe_hist_lut_data lut_data;
    struct msmfb_pipe_qseed_data qseed_data;
    struct mdp_csc csc_data;
    int pic_adj_mode = mode;

    SHDISP_TRACE("in mode = %d, ap_type = %d\n", mode, ap_type);

    if (clmr_trv_info.status == SHDISP_CLMR_TRV_ON) {
        pic_adj_mode = SHDISP_MAIN_DISP_PIC_ADJ_MODE_00;
    }

    memset(&lut_data, 0, sizeof(struct msmfb_pipe_hist_lut_data));
    shdisp_mdp_set_hist_lut_data(pic_adj_mode, &lut_data, ap_type);
    ret = msm_fb_set_pipe_hist_lut(registered_fb[0], &lut_data);
    SHDISP_INFO("MSMFB_SET_PIPE_HIST_LUT : ret = %d.\n", ret);

    memset(&qseed_data, 0, sizeof(struct msmfb_pipe_qseed_data));
    shdisp_mdp_set_qseed_data(pic_adj_mode, &qseed_data, ap_type);
    ret = msm_fb_set_pipe_qseed(registered_fb[0], &qseed_data);
    SHDISP_INFO("MSMFB_SET_PIPE_QSEED : ret = %d.\n", ret);

    memset(&csc_data, 0, sizeof(struct mdp_csc));
    shdisp_mdp_set_csc_data(pic_adj_mode, &csc_data, ap_type);
    ret = msm_fb_set_ccs_matrix(registered_fb[0], &csc_data);
    SHDISP_INFO("MSMFB_SET_CCS_MATRIX : ret = %d.\n", ret);

    SHDISP_TRACE("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_mdp_set_hist_lut_data                                              */
/* ------------------------------------------------------------------------- */
static void shdisp_mdp_set_hist_lut_data(int mode, struct msmfb_pipe_hist_lut_data *data, unsigned short type)
{
    SHDISP_TRACE("in mode = %d, type = %d\n", mode, type);

    data->enable = (unsigned long)pic_adj_enable_tbl[mode][0];

    if (mode != SHDISP_MAIN_DISP_PIC_ADJ_MODE_00) {
        if (type == SHDISP_LCDC_PIC_ADJ_AP_1SEG) {
            SHDISP_INFO("HIST LUT : use hist_lut_data_dtv_1seg_tbl[%d][cont].\n", mode - 1);
            data->color0 = hist_lut_data_dtv_1seg_tbl[mode - 1];
        } else if (type == SHDISP_LCDC_PIC_ADJ_AP_FULLSEG) {
            SHDISP_INFO("HIST LUT : use hist_lut_data_dtv_fullseg_tbl[%d][cont].\n", mode - 1);
            data->color0 = hist_lut_data_dtv_fullseg_tbl[mode - 1];
        } else if (type == SHDISP_LCDC_PIC_ADJ_AP_TMM) {
            SHDISP_INFO("HIST LUT : use hist_lut_data_dtv_tmm_tbl[%d][cont].\n", mode - 1);
            data->color0 = hist_lut_data_dtv_tmm_tbl[mode - 1];
        } else if (type == SHDISP_LCDC_PIC_ADJ_AP_CAM) {
            SHDISP_INFO("HIST LUT : use hist_lut_data_camera_tbl[%d][cont].\n", mode - 1);
            data->color0 = hist_lut_data_camera_tbl[mode - 1];
        } else {
            SHDISP_INFO("HIST LUT : use hist_lut_data_normal_tbl[%d][cont].\n", mode - 1);
            data->color0 = hist_lut_data_normal_tbl[mode - 1];
        }
    } else {
        SHDISP_INFO("HIST LUT : use hist_lut_data_pic_adj_mode_off_tbl[]\n");
        data->color0 = hist_lut_data_pic_adj_mode_off_tbl;
    }

    SHDISP_TRACE("out\n");
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_mdp_set_qseed_data                                                 */
/* ------------------------------------------------------------------------- */
static void shdisp_mdp_set_qseed_data(int mode, struct msmfb_pipe_qseed_data *data, unsigned short type)
{
    SHDISP_TRACE("in mode = %d, type = %d\n", mode, type);

    data->enable = (unsigned long)pic_adj_enable_tbl[mode][1];

    if (mode != SHDISP_MAIN_DISP_PIC_ADJ_MODE_00) {
        if (type == SHDISP_LCDC_PIC_ADJ_AP_1SEG) {
            SHDISP_INFO("QSEED : use qseed_data_dtv_1seg_tbl[%d].\n", mode - 1);
            data->sharp_strength = qseed_data_dtv_1seg_tbl[mode - 1];
        } else if (type == SHDISP_LCDC_PIC_ADJ_AP_FULLSEG) {
            SHDISP_INFO("QSEED : use qseed_data_dtv_fullseg_tbl[%d].\n", mode - 1);
            data->sharp_strength = qseed_data_dtv_fullseg_tbl[mode - 1];
        } else if (type == SHDISP_LCDC_PIC_ADJ_AP_TMM) {
            SHDISP_INFO("QSEED : use qseed_data_dtv_tmm_tbl[%d].\n", mode - 1);
            data->sharp_strength = qseed_data_dtv_tmm_tbl[mode - 1];
        } else if (type == SHDISP_LCDC_PIC_ADJ_AP_CAM) {
            SHDISP_INFO("QSEED : use qseed_data_camera_tbl[%d].\n", mode - 1);
            data->sharp_strength = qseed_data_camera_tbl[mode - 1];
        } else {
            SHDISP_INFO("QSEED : use qseed_data_normal_tbl[%d].\n", mode - 1);
            data->sharp_strength = qseed_data_normal_tbl[mode - 1];
        }
    }
    SHDISP_INFO("QSEED : data->sharp_strength = %d.\n", data->sharp_strength);

    SHDISP_TRACE("out\n");
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_mdp_set_csc_data                                                   */
/* ------------------------------------------------------------------------- */
static void shdisp_mdp_set_csc_data(int mode, struct mdp_csc *data, unsigned short type)
{
    int count;
    __u32 *data_p;

    SHDISP_TRACE("in mode=%d.\n", mode);

    data->enable = (unsigned long)pic_adj_enable_tbl[mode][2];

    if (mode != SHDISP_MAIN_DISP_PIC_ADJ_MODE_00) {
        if (type == SHDISP_LCDC_PIC_ADJ_AP_1SEG) {
            SHDISP_INFO("CSC : use csc_data_dtv_1seg_bt709_tbl[%d].\n", mode - 1);
            data_p = data->csc_mv;
            for (count = 0; count < SHDISP_CSC_MV_SIZE; count++) {
                *data_p++ = csc_data_dtv_1seg_bt709_tbl[mode - 1].matrix_v[count];
            }

            data_p = data->csc_pre_bv;
            for (count = 0; count < SHDISP_CSC_PRE_BV1_SIZE; count++) {
                *data_p++ = csc_data_dtv_1seg_bt709_tbl[mode - 1].pre_bias_v[count];
            }

            data_p = data->csc_post_bv;
            for (count = 0; count < SHDISP_CSC_POST_BV1_SIZE; count++) {
                *data_p++ = csc_data_dtv_1seg_bt709_tbl[mode - 1].post_bias_v[count];
            }

            data_p = data->csc_pre_lv;
            for (count = 0; count < SHDISP_CSC_PRE_LV1_SIZE; count++) {
                *data_p++ = csc_data_dtv_1seg_bt709_tbl[mode - 1].pre_limit_v[count];
            }

            data_p = data->csc_post_lv;
            for (count = 0; count < SHDISP_CSC_POST_LV1_SIZE; count++) {
                *data_p++ = csc_data_dtv_1seg_bt709_tbl[mode - 1].post_limit_v[count];
            }
        } else if (type == SHDISP_LCDC_PIC_ADJ_AP_FULLSEG) {
            SHDISP_INFO("CSC : use csc_data_dtv_fullseg_bt709_tbl[%d].\n", mode - 1);
            data_p = data->csc_mv;
            for (count = 0; count < SHDISP_CSC_MV_SIZE; count++) {
                *data_p++ = csc_data_dtv_fullseg_bt709_tbl[mode - 1].matrix_v[count];
            }

            data_p = data->csc_pre_bv;
            for (count = 0; count < SHDISP_CSC_PRE_BV1_SIZE; count++) {
                *data_p++ = csc_data_dtv_fullseg_bt709_tbl[mode - 1].pre_bias_v[count];
            }

            data_p = data->csc_post_bv;
            for (count = 0; count < SHDISP_CSC_POST_BV1_SIZE; count++) {
                *data_p++ = csc_data_dtv_fullseg_bt709_tbl[mode - 1].post_bias_v[count];
            }

            data_p = data->csc_pre_lv;
            for (count = 0; count < SHDISP_CSC_PRE_LV1_SIZE; count++) {
                *data_p++ = csc_data_dtv_fullseg_bt709_tbl[mode - 1].pre_limit_v[count];
            }

            data_p = data->csc_post_lv;
            for (count = 0; count < SHDISP_CSC_POST_LV1_SIZE; count++) {
                *data_p++ = csc_data_dtv_fullseg_bt709_tbl[mode - 1].post_limit_v[count];
            }
        } else if (type == SHDISP_LCDC_PIC_ADJ_AP_TMM) {
            SHDISP_INFO("CSC : use csc_data_dtv_tmm_bt709_tbl[%d].\n", mode - 1);
            data_p = data->csc_mv;
            for (count = 0; count < SHDISP_CSC_MV_SIZE; count++) {
                *data_p++ = csc_data_dtv_tmm_bt709_tbl[mode - 1].matrix_v[count];
            }

            data_p = data->csc_pre_bv;
            for (count = 0; count < SHDISP_CSC_PRE_BV1_SIZE; count++) {
                *data_p++ = csc_data_dtv_tmm_bt709_tbl[mode - 1].pre_bias_v[count];
            }

            data_p = data->csc_post_bv;
            for (count = 0; count < SHDISP_CSC_POST_BV1_SIZE; count++) {
                *data_p++ = csc_data_dtv_tmm_bt709_tbl[mode - 1].post_bias_v[count];
            }

            data_p = data->csc_pre_lv;
            for (count = 0; count < SHDISP_CSC_PRE_LV1_SIZE; count++) {
                *data_p++ = csc_data_dtv_tmm_bt709_tbl[mode - 1].pre_limit_v[count];
            }

            data_p = data->csc_post_lv;
            for (count = 0; count < SHDISP_CSC_POST_LV1_SIZE; count++) {
                *data_p++ = csc_data_dtv_tmm_bt709_tbl[mode - 1].post_limit_v[count];
            }
        } else if (type == SHDISP_LCDC_PIC_ADJ_AP_CAM) {
            SHDISP_INFO("CSC : use csc_data_camera_bt601_tbl[%d].\n", mode - 1);
            data_p = data->csc_mv;
            for (count = 0; count < SHDISP_CSC_MV_SIZE; count++) {
                *data_p++ = csc_data_camera_bt601_tbl[mode - 1].matrix_v[count];
            }

            data_p = data->csc_pre_bv;
            for (count = 0; count < SHDISP_CSC_PRE_BV1_SIZE; count++) {
                *data_p++ = csc_data_camera_bt601_tbl[mode - 1].pre_bias_v[count];
            }

            data_p = data->csc_post_bv;
            for (count = 0; count < SHDISP_CSC_POST_BV1_SIZE; count++) {
                *data_p++ = csc_data_camera_bt601_tbl[mode - 1].post_bias_v[count];
            }

            data_p = data->csc_pre_lv;
            for (count = 0; count < SHDISP_CSC_PRE_LV1_SIZE; count++) {
                *data_p++ = csc_data_camera_bt601_tbl[mode - 1].pre_limit_v[count];
            }

            data_p = data->csc_post_lv;
            for (count = 0; count < SHDISP_CSC_POST_LV1_SIZE; count++) {
                *data_p++ = csc_data_camera_bt601_tbl[mode - 1].post_limit_v[count];
            }
        } else {
            SHDISP_INFO("CSC : use csc_data_normal_bt601_tbl[%d].\n", mode - 1);
            data_p = data->csc_mv;
            for (count = 0; count < SHDISP_CSC_MV_SIZE; count++) {
                *data_p++ = csc_data_normal_bt601_tbl[mode - 1].matrix_v[count];
            }

            data_p = data->csc_pre_bv;
            for (count = 0; count < SHDISP_CSC_PRE_BV1_SIZE; count++) {
                *data_p++ = csc_data_normal_bt601_tbl[mode - 1].pre_bias_v[count];
            }

            data_p = data->csc_post_bv;
            for (count = 0; count < SHDISP_CSC_POST_BV1_SIZE; count++) {
                *data_p++ = csc_data_normal_bt601_tbl[mode - 1].post_bias_v[count];
            }

            data_p = data->csc_pre_lv;
            for (count = 0; count < SHDISP_CSC_PRE_LV1_SIZE; count++) {
                *data_p++ = csc_data_normal_bt601_tbl[mode - 1].pre_limit_v[count];
            }

            data_p = data->csc_post_lv;
            for (count = 0; count < SHDISP_CSC_POST_LV1_SIZE; count++) {
                *data_p++ = csc_data_normal_bt601_tbl[mode - 1].post_limit_v[count];
            }
        }
    }

    SHDISP_TRACE("out\n");
    return;
}
#endif



#if defined(USE_LINUX) || (defined(SHDISP_CLMR_FW_TIMEOUT_DUMP) && defined(SHDISP_DBG_BOOTLOG_REGDUMP))
/* ------------------------------------------------------------------------- */
/* shdisp_clmr_info_reg_dump                                                 */
/* ------------------------------------------------------------------------- */
static void shdisp_clmr_info_reg_dump(void)
{
    shdisp_clmr_reg_dump(SHDISP_CLMR_REG_INFOREG0);
    shdisp_clmr_reg_dump(SHDISP_CLMR_REG_INFOREG1);
    shdisp_clmr_reg_dump(SHDISP_CLMR_REG_INFOREG2);
    shdisp_clmr_reg_dump(SHDISP_CLMR_REG_INFOREG3);
    shdisp_clmr_reg_dump(SHDISP_CLMR_REG_ARMINFOREG0);
    shdisp_clmr_reg_dump(SHDISP_CLMR_REG_ARMINFOREG1);
    shdisp_clmr_reg_dump(SHDISP_CLMR_REG_ARMINFOREG2);
    shdisp_clmr_reg_dump(SHDISP_CLMR_REG_ARMINFOREG3);
    shdisp_clmr_reg_dump(SHDISP_CLMR_REG_TESTMODE12);
    shdisp_clmr_reg_dump(SHDISP_CLMR_REG_TESTMODE13);
    shdisp_clmr_reg_dump(SHDISP_CLMR_REG_TESTMODE14);
    shdisp_clmr_reg_dump(SHDISP_CLMR_REG_TESTMODE15);
    shdisp_clmr_reg_dump(SHDISP_CLMR_REG_DEVCODE);
}
#endif /* defined(USE_LINUX) || (defined(SHDISP_CLMR_FW_TIMEOUT_DUMP) && defined(SHDISP_DBG_BOOTLOG_REGDUMP)) */
/* ------------------------------------------------------------------------- */
/* shdisp_clmr_fw_timeout_dump                                               */
/* ------------------------------------------------------------------------- */
static void shdisp_clmr_fw_timeout_dump(void)
{
    struct shdisp_dbg_ptrinfo ring_dump_buf = {0};
    struct shdisp_dbg_ptrinfo stack_dump_buf = {0};

#ifdef SHDISP_CLMR_FW_TIMEOUT_DUMP
    ring_dump_buf.ptr = kzalloc(LOG_BUF_SIZE, GFP_KERNEL);
    if (ring_dump_buf.ptr == NULL) {
        SHDISP_ERR("allocate ringbuffer dump buffer error. [no memory]\n");
    } else {
        ring_dump_buf.length = LOG_BUF_SIZE;
        ring_dump_buf.need_free = 1;
    }

    stack_dump_buf.ptr = kzalloc(LOG_BUF_SIZE_ST, GFP_KERNEL);
    if (stack_dump_buf.ptr == NULL) {
        SHDISP_ERR("allocate stack dump buffer error. [no memory]\n");
    } else {
        stack_dump_buf.length = LOG_BUF_SIZE_ST;
        stack_dump_buf.need_free = 1;
    }
#endif /* SHDISP_CLMR_FW_TIMEOUT_DUMP */

    shdisp_dbg_ringbuffer_dump(ring_dump_buf.ptr);

    stack_dump_buf.length =
        shdisp_dbg_stacktrace_dump(stack_dump_buf.ptr, stack_dump_buf.length);

#ifdef SHDISP_CLMR_FW_TIMEOUT_DUMP
    if (shdisp_clmr_fw_timeout_RegDump(&ring_dump_buf, &stack_dump_buf) != 0) {
        if (ring_dump_buf.ptr != NULL) {
            kfree(ring_dump_buf.ptr);
        }
        if (stack_dump_buf.ptr != NULL) {
            kfree(stack_dump_buf.ptr);
        }
    }
#endif /* SHDISP_CLMR_FW_TIMEOUT_DUMP */
}
/* ------------------------------------------------------------------------- */
/* shdisp_clmr_reg_dump_logset                                               */
/* ------------------------------------------------------------------------- */
static void shdisp_clmr_reg_dump_logset(void)
{
    shdisp_clmr_info_reg_dump();
    shdisp_clmr_fw_timeout_dump();

    shdisp_SYS_FWCMD_set_timeoutexception(1);
    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_clmr_api_set_pwroff_fwlog_dump_flg                                 */
/* ------------------------------------------------------------------------- */
void shdisp_clmr_api_set_pwroff_fwlog_dump_flg(int flg)
{
#if defined(CONFIG_ANDROID_ENGINEERING)
    shdisp_clmr_ctrl.pwroff_fwlog_dump_flg = flg;
#endif /* defined (CONFIG_ANDROID_ENGINEERING) */
}


/* ------------------------------------------------------------------------- */
/* shdisp_clmr_pwroff_fwlog_dump                                             */
/* ------------------------------------------------------------------------- */
static void shdisp_clmr_pwroff_fwlog_dump(void)
{
#if defined(CONFIG_ANDROID_ENGINEERING)
    if (!shdisp_clmr_ctrl.pwroff_fwlog_dump_flg) {
        return;
    }
    shdisp_clmr_FWLog_readAndDump(0);
#endif /* defined (CONFIG_ANDROID_ENGINEERING) */
}

#if defined(CONFIG_ANDROID_ENGINEERING)
/* ------------------------------------------------------------------------- */
/* shdisp_clmr_FWLogLength                                                   */
/* ------------------------------------------------------------------------- */
static inline unsigned int shdisp_clmr_FWLogLength(void)
{
    return (CALI_LOGREAD_HOSTAEY_VAL + 1) * 128 / 8;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_FWLog_chronological                                           */
/* ------------------------------------------------------------------------- */
static unsigned int shdisp_clmr_FWLog_chronological(unsigned char *dst, unsigned char *src,
                                        unsigned int wp, unsigned int rp )
{
    const unsigned int loglength = shdisp_clmr_FWLogLength();
    unsigned int rtn = 0;

    if ((wp > loglength) || (wp == 0)) {
        return rtn;
    }

    if (wp == rp) {
        memcpy( dst,                    src + wp, loglength - wp );
        memcpy( dst + (loglength - wp), src     , wp             );
        rtn = loglength;

    } else {
        memcpy( dst, src, wp );
        rtn = wp;
    }

    return rtn;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_FWLog_get                                                     */
/* ------------------------------------------------------------------------- */
static unsigned int shdisp_clmr_FWLog_get(unsigned char *buf, unsigned int len, int iseDramPtrRst)
{
    static unsigned char *tempbuf = 0;
    unsigned char *fwlog;
    unsigned int infreg0, rp, wp;
    int i = 0;
    unsigned int rtn = 0;
    unsigned char intm_temp[4] = {0};
    unsigned char intm_off[4] = {0};
    const unsigned int loglength = shdisp_clmr_FWLogLength();


    if (!tempbuf) {
        tempbuf = kmalloc(loglength, GFP_KERNEL | __GFP_DMA);
    }

    if (!tempbuf) {
        SHDISP_ERR("temp buffer alloc failure!! \n");
        return rtn;
    }

    shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_REG_INFOREG0, NULL, 0, (unsigned char*)&infreg0, 4 );
    infreg0 = htonl(infreg0);

    wp = (infreg0 >> 8)  & 0x000000FF;
    rp = (infreg0 >> 16) & 0x000000FF;
    if ((wp > loglength) || (wp == 0)) {
        SHDISP_ERR("fwlog dump failed. wp=%d\n", wp );
        return rtn;
    }
    fwlog = ( wp == rp ) ? tempbuf : buf;
    memset( fwlog, 0, loglength );

    SHDISP_ERR(" infreg0 = 0x%08x, wp = 0x%08x, rp = 0x%08x\n", infreg0, wp, rp );
    wp *= 128;
    rp *= 128;

    shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_REG_INTM, NULL, 0, intm_temp, 4 );
    shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_REG_INTM, intm_off, 4, NULL, 0 );

    shdisp_clmr_disable_irq();

    for (i = 0; i != ARRAY_SIZE(dump_fw_log_Setting); i++) {
        shdisp_clmr_regSet(&dump_fw_log_Setting[i]);
    }

    shdisp_SYS_clmr_sio_eDram_transfer(SHDISP_CLMR_EDRAM_C9, fwlog, loglength );

    shdisp_clmr_enable_irq();

    shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_REG_INTM, intm_temp, 4, NULL, 0 );

    if (iseDramPtrRst) {
        shdisp_FWCMD_eDramPtrReset();
    }

    if (wp == rp) {
        rtn = shdisp_clmr_FWLog_chronological(buf, tempbuf, wp, rp);
    } else {
        rtn = wp;
    }

    return rtn;
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_FWLogDump                                                     */
/* ------------------------------------------------------------------------- */
static void shdisp_clmr_FWLogDump(unsigned char *buf, const unsigned int length)
{
    unsigned char *chkpos = buf;
    unsigned int cpyhead = 0;
    unsigned int cpylen = 0;
    unsigned int i = 0;
    unsigned int printcnt = 0;
    unsigned char printtemp[256];


    SHDISP_ERR("called. \n" );
    for (i = 0; i != length; i++) {
        cpylen++;

        if ((0x0a == *chkpos)) {
            if (cpylen > sizeof(printtemp) - 1) {
                memcpy( printtemp, buf + cpyhead, sizeof(printtemp) - 5);
                *(printtemp + sizeof(printtemp) - 5) =
                *(printtemp + sizeof(printtemp) - 4) =
                *(printtemp + sizeof(printtemp) - 3) = '.';
                *(printtemp + sizeof(printtemp) - 2) = '\n';
                *(printtemp + sizeof(printtemp) - 1) = '\0';
            } else {
                memcpy( printtemp, buf + cpyhead, cpylen );
                *(printtemp + cpylen) = '\0';
            }

            SHDISP_ERR( "[%05d - %05d]: %s", cpyhead, (cpyhead + cpylen - 1), printtemp );
            printcnt++;

            shdisp_SYS_delay_us(1000);

            cpyhead = cpyhead + cpylen;
            cpylen = 0;
        }
        chkpos++;
    }

    if (!printcnt) {
        SHDISP_ERR("fwlog nothing... no print\n" );
    }


    SHDISP_ERR("done. \n" );
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_FWLog_readAndDump                                             */
/* ------------------------------------------------------------------------- */
unsigned int shdisp_clmr_FWLog_readAndDump(int iseDramPtrRst)
{
    static unsigned char *buf = 0;
    unsigned int readlength = 0;
    const int buflength = shdisp_clmr_FWLogLength();

    if (!buf) {
        buf = kmalloc(buflength, GFP_KERNEL | __GFP_DMA);
    }

    if (!buf) {
        SHDISP_ERR("log buffer alloc failure!! \n");
        return SHDISP_RESULT_FAILURE;
    }

    readlength = shdisp_clmr_FWLog_get(buf, buflength, iseDramPtrRst);

    if (readlength == 0) {
        return SHDISP_RESULT_FAILURE;
    }

    shdisp_clmr_FWLogDump(buf, readlength);
    return SHDISP_RESULT_SUCCESS;
}
#endif /* defined (CONFIG_ANDROID_ENGINEERING) */

/* ------------------------------------------------------------------------- */
/* shidsp_clmr_image_get_preptg_value                                        */
/* ------------------------------------------------------------------------- */
static void shidsp_clmr_image_get_preptg_value( unsigned int *premetbl, unsigned int *premetbh,
                                                unsigned int *ptgmdtbl, unsigned int *ptgmdtbh )
{
    static struct {
        unsigned short regs;
        unsigned char rbuf[4];
    } ary[4] = {
        { SHDISP_CLMR_REG_PREMOCOETBL },
        { SHDISP_CLMR_REG_PREMOCOETBH },
        { SHDISP_CLMR_REG_PTGMOCODTBL },
        { SHDISP_CLMR_REG_PTGMOCODTBH },
    };

    unsigned int *data[4] = { premetbl, premetbh, ptgmdtbl, ptgmdtbh };
    int i = 0;

    for (; i != ARRAY_SIZE(ary); ++i) {
        shdisp_SYS_clmr_sio_transfer(ary[i].regs, NULL, 0, ary[i].rbuf, 4 );
        *(data[i]) = ntohl(*((unsigned int *)ary[i].rbuf));
    }
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_image_preptg_dump                                             */
/* ------------------------------------------------------------------------- */
static void shdisp_clmr_image_preptg_dump( unsigned int premetbl, unsigned int premetbh,
                                                unsigned int ptgmdtbl, unsigned int ptgmdtbh )
{
    const int mask2915 = 0x3fff8000;
    const int mask1400 = 0x00007fff;
    char sbufpre[64];
    char sbufptg[64];
    unsigned int frame, pre5930, pre2900, ptg5930, ptg2900, temp;

    pre5930     = premetbl & mask2915;

    temp        = (premetbl & mask1400);
    temp      <<= 15;
    ptg5930     = (temp & mask2915);

    temp        = (premetbh >> 15) & mask1400;
    pre5930    |= temp;

    ptg5930    |= premetbh & mask1400;

    pre2900     = ptgmdtbl & mask2915;

    temp        = (ptgmdtbl & mask1400);
    temp      <<= 15;
    ptg2900     = (temp & mask2915);

    temp        = (ptgmdtbh >> 15 ) & mask1400;
    pre2900    |= temp;

    ptg2900    |= ptgmdtbh & mask1400;

    frame = (premetbl >> 30) & 0x00000003;

    shdisp_dbg_i2bit( pre5930, sbufpre,      31, 29, 0);
    shdisp_dbg_i2bit( pre2900, sbufpre + 30, 31, 29, 0);
    shdisp_dbg_i2bit( ptg5930, sbufptg,      31, 29, 0);
    shdisp_dbg_i2bit( ptg2900, sbufptg + 30, 31, 29, 0);
    SHDISP_DEBUG( "Frame[%d][oldest   >         >         >         >         >    latest]\n", frame );
    SHDISP_DEBUG( "[IN     :%s]\n", sbufpre );
    SHDISP_DEBUG( "[OUT    :%s]\n", sbufptg );
}

/* ------------------------------------------------------------------------- */
/* shdisp_clmr_image_preptg_ReadAndDump                                      */
/* ------------------------------------------------------------------------- */
void shdisp_clmr_image_preptg_ReadAndDump(void)
{
    unsigned int premetbl, premetbh, ptgmdtbl, ptgmdtbh;
    int isSecFrame, temp;

    shidsp_clmr_image_get_preptg_value(&premetbl, &premetbh, &ptgmdtbl, &ptgmdtbh );

    temp = premetbl & 0xc0000000;
    isSecFrame = 1;
    isSecFrame &= (temp == (premetbh & 0xc0000000));
    isSecFrame &= (temp == (ptgmdtbl & 0xc0000000));
    isSecFrame &= (temp == (ptgmdtbh & 0xc0000000));

    if (isSecFrame) {
        shdisp_clmr_image_preptg_dump(premetbl, premetbh, ptgmdtbl, ptgmdtbh);
    } else {
        SHDISP_DEBUG( "1secFrame get failed.\n" );
    }
}

