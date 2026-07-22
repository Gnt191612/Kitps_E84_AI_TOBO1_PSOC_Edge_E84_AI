/**
 * @file ov7675_regs.h
 * @brief OV7675 鎽勫儚澶村瘎瀛樺櫒鏄犲皠 鈥?QQVGA (160x120) Y 閫氶亾妯″紡閰嶇疆
 *
 * 鍙傝€?OV7675 datasheet (OmniVision) + Application Notes.
 *
 * 褰撳墠瀵勫瓨鍣ㄩ厤缃洰鏍囷細
 *   - 杈撳嚭鏍煎紡: Y (鐏板害) / Raw RGB
 *   - 鍒嗚鲸鐜? QQVGA 160x120
 *   - 杈撳嚭: 閫愯, PCLK rising edge 閲囨牱
 *   - FPS: ~15-30 (鐢?XCLK 棰戠巼 + 鍐呴儴 PLL 鍐冲畾)
 *
 * 鈿狅笍 濡傛灉浣跨敤杩囩▼涓浘鍍忓紓甯革紙鍋忚壊/閿欎綅/鍏ㄩ粦/闂儊锛夛紝
 *    璇风敤閫昏緫鍒嗘瀽浠姄鍙?VSYNC/HREF/PCLK 鏃跺簭纭锛屾垨鍙傝€? *    OV7670 Application Notes 璋冩暣瀵勫瓨鍣ㄥ€笺€? */

#ifndef OV7675_REGS_H
#define OV7675_REGS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== */
/* OV7675 鍣ㄤ欢鍦板潃锛圫CCB 7-bit锛?                                           */
/* ======================================================================== */
#define OV7675_ADDR             0x42    /* 鍐欏湴鍧€ (0x21 << 1) */
#define OV7675_ADDR_READ        0x43    /* 璇诲湴鍧€ */

/* ======================================================================== */
/* 鍏抽敭瀵勫瓨鍣ㄥ湴鍧€                                                            */
/* ======================================================================== */

#define REG_GAIN                0x00    /* AGC 澧炵泭 */
#define REG_BLUE                0x01    /* AWB 钃濊壊閫氶亾 */
#define REG_RED                 0x02    /* AWB 绾㈣壊閫氶亾 */
#define REG_VREF                0x03    /* 鍨傜洿鍙傝€?*/
#define REG_COM1                0x04    /* 閫氱敤鎺у埗 1 */
#define REG_BAVE                0x05    /* 钃濊壊閫氶亾骞冲潎鍊?*/
#define REG_GbAVE               0x06    /* Gb 閫氶亾骞冲潎鍊?*/
#define REG_AECHH               0x07    /* 鏇濆厜楂樹綅 */
#define REG_RAVE                0x08    /* 绾㈣壊骞冲潎鍊?*/
#define REG_COM2                0x09    /* 閫氱敤鎺у埗 2 */
#define REG_PID                 0x0A    /* 浜у搧 ID 楂樺瓧鑺?(0x76) */
#define REG_VER                 0x0B    /* 浜у搧 ID 浣庡瓧鑺?(0x73) */
#define REG_COM3                0x0C    /* 閫氱敤鎺у埗 3 */
#define REG_COM4                0x0D    /* 閫氱敤鎺у埗 4 */
#define REG_COM5                0x0E    /* 閫氱敤鎺у埗 5 */
#define REG_COM6                0x0F    /* 閫氱敤鎺у埗 6 */
#define REG_AECH                0x10    /* 鏇濆厜鍊?*/
#define REG_CLKRC               0x11    /* 鏃堕挓鎺у埗 */
#define REG_COM7                0x12    /* 閫氱敤鎺у埗 7 */
#define REG_COM8                0x13    /* 閫氱敤鎺у埗 8 */
#define REG_COM9                0x14    /* 閫氱敤鎺у埗 9 */
#define REG_COM10               0x15    /* 閫氱敤鎺у埗 10 */
#define REG_HSTART              0x17    /* 姘村钩璧峰 */
#define REG_HSTOP               0x18    /* 姘村钩缁撴潫 */
#define REG_VSTART              0x19    /* 鍨傜洿璧峰 */
#define REG_VSTOP               0x1A    /* 鍨傜洿缁撴潫 */
#define REG_PSHFT               0x1B    /* 鍍忕礌鍋忕Щ */
#define REG_MIDH                0x1C    /* 鍘傚 ID 楂?*/
#define REG_MIDL                0x1D    /* 鍘傚 ID 浣?*/
#define REG_MVFP                0x1E    /* 闀滃儚/缈昏浆 */
#define REG_LAEC                0x1F    /* 鑷姩鏇濆厜鎺у埗 */
#define REG_ADC                 0x20    /* ADC 鎺у埗 */
#define REG_ALAW                0x21    /* AWB/AGC 鎺у埗 */
#define REG_COM11               0x22    /* 閫氱敤鎺у埗 11 */
#define REG_COM12               0x23    /* 閫氱敤鎺у埗 12 */
#define REG_COM13               0x24    /* 閫氱敤鎺у埗 13 */
#define REG_COM14               0x25    /* 閫氱敤鎺у埗 14 */
#define REG_EDGE                0x26    /* 杈圭紭澧炲己 */
#define REG_COM15               0x27    /* 閫氱敤鎺у埗 15 */
#define REG_COM16               0x28    /* 閫氱敤鎺у埗 16 */
#define REG_COM17               0x29    /* 閫氱敤鎺у埗 17 */
#define REG_AEW                 0x24    /* AEC 涓婇檺 */
#define REG_AEB                 0x25    /* AEC 涓嬮檺 */
#define REG_VPT                 0x26    /* AEC 蹇€熸ā寮?*/
#define REG_RSVD_27             0x27    /* 淇濈暀 */
#define REG_HREF                0x32    /* HREF 鎺у埗 */
#define REG_CHLF                0x33    /* 閫氶亾澧炵泭浣?*/
#define REG_ARBLM               0x34    /* 浠茶/閿佸畾 */
#define REG_RSVD_35             0x35    /* 淇濈暀 */
#define REG_RSVD_36             0x36    /* 淇濈暀 */
#define REG_ADC                 0x37    /* ADC 鎺у埗 */
#define REG_ACOM                0x38    /* ADC 鎺у埗 */
#define REG_OFON                0x39    /* 鍋忕Щ */
#define REG_TSLB                0x3A    /* 杈撳嚭鏍煎紡 */
#define REG_COM18               0x3B    /* 閫氱敤鎺у埗 18 */
#define REG_COM19               0x3C    /* 閫氱敤鎺у埗 19 */
#define REG_COM20               0x3D    /* 閫氱敤鎺у埗 20 */
#define REG_COM21               0x3E    /* 閫氱敤鎺у埗 21 */
#define REG_COM22               0x3F    /* 閫氱敤鎺у埗 22 */
#define REG_COM23               0x40    /* 閫氱敤鎺у埗 23 */
#define REG_DBLC1               0x43    /* 榛戠數骞宠ˉ鍋?*/
#define REG_DBLC2               0x44    /* 榛戠數骞宠ˉ鍋?*/
#define REG_DBLC3               0x45    /* 榛戠數骞宠ˉ鍋?*/
#define REG_DBLC4               0x46    /* 榛戠數骞宠ˉ鍋?*/
#define REG_DBLC5               0x47    /* 榛戠數骞宠ˉ鍋?*/
#define REG_DBLC6               0x48    /* 榛戠數骞宠ˉ鍋?*/
#define REG_DBLC7               0x49    /* 榛戠數骞宠ˉ鍋?*/
#define REG_DBLC8               0x4A    /* 榛戠數骞宠ˉ鍋?*/
#define REG_DBLC9               0x4B    /* 榛戠數骞宠ˉ鍋?*/
#define REG_DBLC10              0x4C    /* 榛戠數骞宠ˉ鍋?*/
#define REG_DBLC11              0x4D    /* 榛戠數骞宠ˉ鍋?*/
#define REG_DBLC12              0x4E    /* 榛戠數骞宠ˉ鍋?*/
#define REG_DBLC13              0x4F    /* 榛戠數骞宠ˉ鍋?*/
#define REG_RSVD_50             0x50    /* 淇濈暀 */
#define REG_RSVD_51             0x51    /* 淇濈暀 */
#define REG_RSVD_52             0x52    /* 淇濈暀 */
#define REG_RSVD_53             0x53    /* 淇濈暀 */
#define REG_AEH                 0x55    /* AEC 涓婇檺 */
#define REG_AEL                 0x56    /* AEC 涓嬮檺 */
#define REG_RSVD_57             0x57    /* 淇濈暀 */
#define REG_RSVD_58             0x58    /* 淇濈暀 */
#define REG_RSVD_59             0x59    /* 淇濈暀 */
#define REG_RSVD_5A             0x5A    /* 淇濈暀 */
#define REG_RSVD_5B             0x5B    /* 淇濈暀 */
#define REG_RSVD_5C             0x5C    /* 淇濈暀 */
#define REG_RSVD_5D             0x5D    /* 淇濈暀 */
#define REG_RSVD_5E             0x5E    /* 淇濈暀 */
#define REG_RSVD_5F             0x5F    /* 淇濈暀 */
#define REG_COM24               0x60    /* 閫氱敤鎺у埗 24 */
#define REG_COM25               0x61    /* 閫氱敤鎺у埗 25 */
#define REG_COM26               0x62    /* 閫氱敤鎺у埗 26 */
#define REG_COM27               0x63    /* 閫氱敤鎺у埗 27 */
#define REG_COM28               0x64    /* 閫氱敤鎺у埗 28 */
#define REG_COM29               0x65    /* 閫氱敤鎺у埗 29 */
#define REG_COM30               0x66    /* 閫氱敤鎺у埗 30 */
#define REG_COM31               0x67    /* 閫氱敤鎺у埗 31 */
#define REG_COM32               0x68    /* 閫氱敤鎺у埗 32 */
#define REG_COM33               0x69    /* 閫氱敤鎺у埗 33 */
#define REG_COM34               0x6A    /* 閫氱敤鎺у埗 34 */
#define REG_COM35               0x6B    /* 閫氱敤鎺у埗 35 */
#define REG_COM36               0x6C    /* 閫氱敤鎺у埗 36 */
#define REG_COM37               0x6D    /* 閫氱敤鎺у埗 37 */
#define REG_COM38               0x6E    /* 閫氱敤鎺у埗 38 */
#define REG_COM39               0x6F    /* 閫氱敤鎺у埗 39 */
#define REG_COM40               0x70    /* 閫氱敤鎺у埗 40 */
#define REG_COM41               0x71    /* 閫氱敤鎺у埗 41 */
#define REG_COM42               0x72    /* 閫氱敤鎺у埗 42 */
#define REG_COM43               0x73    /* 閫氱敤鎺у埗 43 */
#define REG_COM44               0x74    /* 閫氱敤鎺у埗 44 */
#define REG_COM45               0x75    /* 閫氱敤鎺у埗 45 */
#define REG_COM46               0x76    /* 閫氱敤鎺у埗 46 */
#define REG_COM47               0x77    /* 閫氱敤鎺у埗 47 */
#define REG_COM48               0x78    /* 閫氱敤鎺у埗 48 */
#define REG_COM49               0x79    /* 閫氱敤鎺у埗 49 */
#define REG_COM50               0x7A    /* 閫氱敤鎺у埗 50 */
#define REG_COM51               0x7B    /* 閫氱敤鎺у埗 51 */
#define REG_COM52               0x7C    /* 閫氱敤鎺у埗 52 */
#define REG_COM53               0x7D    /* 閫氱敤鎺у埗 53 */
#define REG_COM54               0x7E    /* 閫氱敤鎺у埗 54 */
#define REG_COM55               0x7F    /* 閫氱敤鎺у埗 55 */

/* ======================================================================== */
/* COM7 (0x12) 鈥?Common Control 7 鈥?鍏抽敭閰嶇疆瀵勫瓨鍣?                         */
/* ======================================================================== */
#define COM7_RESET              0x80    /* 杞欢澶嶄綅 */
#define COM7_FMT_MASK           0x38    /* 杈撳嚭鏍煎紡鎺╃爜 */
#define COM7_FMT_Y              0x00    /* Y-only (鐏板害) 杈撳嚭 */
#define COM7_FMT_RGB            0x04    /* RGB 杈撳嚭 */
#define COM7_FMT_BAYER          0x01    /* Raw Bayer RGB */
#define COM7_FMT_P_BAYER        0x02    /* Processed Bayer RGB */
#define COM7_FMT_RGB565         0x04    /* RGB565 */
#define COM7_FMT_RGB555         0x0C    /* RGB555 */
#define COM7_FMT_RGB444         0x10    /* RGB444 */
#define COM7_FMT_YUV            0x08    /* YUV 杈撳嚭 (Y channel = 鐏板害) */
#define COM7_FMT_P_YUV          0x0E    /* Processed YUV */
#define COM7_RES_MASK           0x07    /* 鍒嗚鲸鐜囨帺鐮?*/
#define COM7_RES_VGA            0x00    /* VGA */
#define COM7_RES_CIF            0x01    /* CIF */
#define COM7_RES_QVGA           0x02    /* QVGA, 320x240 */
#define COM7_RES_QCIF           0x03    /* QCIF */
#define COM7_RES_NTSC           0x04    /* NTSC */
#define COM7_RES_PAL            0x05    /* PAL */
#define COM7_RES_QQVGA          0x06    /* QQVGA (160x120) 鈥?鎴戜滑鐨勭洰鏍?*/
#define COM7_RES_QQCIF          0x07    /* QQCIF */

/* ======================================================================== */
/* COM3 (0x0C) 鈥?Common Control 3                                           */
/* ======================================================================== */
#define COM3_VSYNC              0x40    /* VSYNC 鏋佹€? 1=楂樻湁鏁?*/
#define COM3_HREF               0x20    /* HREF 鏋佹€? 1=楂樻湁鏁?(榛樿) */
#define COM3_SWAP               0x10    /* 浜ゆ崲瀛楄妭搴?*/

/* ======================================================================== */
/* COM15 (0x27) 鈥?Common Control 15 鈥?杈撳嚭鏍煎紡                              */
/* ======================================================================== */
#define COM15_RGBFMT_MASK       0xC0    /* RGB 鏍煎紡鎺╃爜 */
#define COM15_RGBFMT_565        0x00    /* RGB565 */
#define COM15_RGBFMT_555        0x40    /* RGB555 */
#define COM15_RGBFMT_444        0x80    /* RGB444 */

/* ======================================================================== */
/* MVFP (0x1E) 鈥?Mirror/VFlip/Polarity                                      */
/* ======================================================================== */
#define MVFP_MIRROR             0x20    /* 姘村钩闀滃儚 */
#define MVFP_VFLIP              0x10    /* 鍨傜洿缈昏浆 */

/* ======================================================================== */
/* TSLB (0x3A) 鈥?杈撳嚭鏃跺簭鎺у埗                                               */
/* ======================================================================== */
#define TSLB_YLAST              0x04    /* Y 閫氶亾涓烘渶鍚庝竴涓瓧鑺?*/

/* ======================================================================== */
/* CLKRC (0x11) 鈥?鏃堕挓鎺у埗                                                  */
/* ======================================================================== */
#define CLKRC_INTERNAL          0x80    /* 浣跨敤鍐呴儴鏃堕挓 (鍊嶉) */
#define CLKRC_DIV_MASK          0x3F    /* 鍒嗛鎺╃爜 */

/* ======================================================================== */
/* AGC/澧炵泭鐩稿叧                                                              */
/* ======================================================================== */
#define COM8_FAST_AEC           0x80    /* 蹇€?AEC 鏇存柊 */
#define COM8_AEC                0x40    /* 鑷姩鏇濆厜浣胯兘 */
#define COM8_AEC_STEP           0x20    /* AEC 姝ラ暱 */
#define COM8_AGC                0x04    /* 鑷姩澧炵泭浣胯兘 */
#define COM8_AWB                0x02    /* 鑷姩鐧藉钩琛′娇鑳?*/

/* ======================================================================== */
/* HREF (0x32) 鈥?HREF 鎺у埗                                                   */
/* ======================================================================== */
#define HREF_EDGE               0x80    /* HREF 杈规部瑙﹀彂 */

#ifdef __cplusplus
}
#endif

#endif /* OV7675_REGS_H */
