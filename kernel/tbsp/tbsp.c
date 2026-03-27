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

/************************�ިi �ߧ� �n��******************************/

/* @<MOD_HEAD */
#include <ttos.h>
#include <ttosBase.h>
#include <ttosHal.h>
/* @MOD_HEAD> */

/************************���a? ���a? ���k��******************************/
/************************���d�m����?���a?���k��******************************/
/************************���W�̧����[���רc���ܨ�******************************/
T_VOID  tbspInitTimer (T_VOID);
T_UWORD cpuNumGet (T_VOID);

/************************���ب���??���רc���ܨ�******************************/
/************************���[�U��?�֧�?�ܧ���?******************************/
/************************�����[���d����?�ܧ���?******************************/

/* @<MOD_VAR */

/* @MOD_VAR> */

/************************���a��    �����c******************************/

/* @MODULE> */

/*
 * @brief:
 *    ����?���Z?���{��TBSP���n�{���p?���_�U���بZ���U�{���m�X���j?��?�����o��:<br>
 *    - �����i��?�̧���?��?�§��j�`���̨`����c��?�����n?<br>
 *    - ���a�ا��ש�����?��?��tick���j�`���̨`���W����?�����Ȩo��?�c���n?<br>
 *    - ���o?�����o����?��?��tick���j�`���̨`���n?<br>
 * @return:
 *    ���֨T
 * @tracedREQ: RT
 * @implements: DT.6.7
 */
T_VOID tbspInit (T_VOID)
{

    /*
     * @KEEP_COMMENT:
     * ���c�������[tbspInitVTimer(DT.6.13)���a�ا��ש�����?��?��tick���j�`���̨`���W����?�����Ȩo��?�c�����{���o?�����o
     * ����?��?��tick���j�`���̨`
     */
    tbspInitTimer ();
#if CONFIG_SMP == 1
    /* @KEEP_COMMENT:
     * ���c�����o?���ب�cpu��?�i����T�����X���Шccpu���o?�����o��?����?�Ч��j�`
     */
    CPU_SET (cpuid_get (), TTOS_CPUSET_ENABLED ());
    CPU_ZERO (TTOS_CPUSET_RESERVED ());
    ttosConfigCpuNum = cpuNumGet ();
#endif
}
