// 全局调试开关声明
#ifndef DEBUG_H
/**
 * @file debug.h
 * @brief 调试宏与辅助函数的声明，便于在开发过程中打印和控制调试信息。
 *
 * 详细说明: 该文件通常包含调试开关、日志宏或轻量的断言/打印辅助函数。
 *
 * 使用示例:
 * @code
 * #include "debug.h"
 * DEBUG_PRINT("some debug info");
 * @endcode
 */
#define DEBUG_H

extern int debug; // 0: 关闭 qDebug 输出, 1: 打开
/**
 * @brief 全局调试开关
 *
 * 设为 0 表示关闭调试输出（`qDebug()` 等），设为 1 表示开启。
 *
 * 使用示例:
 * @code
 * debug = 1; // 打开调试输出
 * qDebug() << "调试信息";
 * @endcode
 */

#endif // DEBUG_H
