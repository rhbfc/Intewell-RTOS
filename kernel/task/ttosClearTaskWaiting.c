/*
 * Copyright (c) 2026 Kyland Inc.
 * Intewell-RTOS is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

/* @<MODULE */

/************************Í· ÎÄ ¼þ******************************/

/* @<MOD_HEAD */
#include <ttosBase.h>
#include <ttosInterHal.h>
/* @MOD_HEAD> */

/************************ºê ¶¨ Òå******************************/
/************************ÀàÐÍ¶¨Òå******************************/
/************************Íâ²¿ÉùÃ÷******************************/
/************************Ç°ÏòÉùÃ÷******************************/
/************************Ä£¿é±äÁ¿******************************/
/************************È«¾Ö±äÁ¿******************************/
/************************Êµ   ÏÖ*******************************/

/* @MODULE> */

/*
 * @brief:
 *    Çå³ýÈÎÎñµÄµÈ´ý×´Ì¬¡£
 * @param[out]: task: ÈÎÎñ¿ØÖÆ¿é
 * @return:
 *    ÎÞ
 * @implements: RTE_DTASK.3.1
 */
void ttosClearTaskWaiting (T_TTOS_TaskControlBlock *task)
{
    T_UWORD state = (T_UWORD)TTOS_TASK_WAITING;
    state         = ~state;
    /* @KEEP_COMMENT: Çå³ý<task>ÈÎÎñµÄTTOS_TASK_WAITING×´Ì¬ */
    task->state &= state;

    /* @REPLACE_BRACKET: <task>ÈÎÎñ×´Ì¬²»ÊÇTTOS_TASK_NONRUNNING_STATE */
    state = (T_UWORD)TTOS_TASK_NONRUNNING_STATE;
    if (0 == (task->state & state))
    {
        /* @KEEP_COMMENT: µ÷ÓÃttosSetTaskReady(DT.2.32)ÉèÖÃ<task>ÈÎÎñÎª¾ÍÐ÷ÈÎÎñ
         */
        (void)ttosSetTaskReady (task);
    }
}
