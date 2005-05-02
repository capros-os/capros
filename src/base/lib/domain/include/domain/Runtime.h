#ifndef __RUNTIME_H__
#define __RUNTIME_H__

/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

/*
 * This header captures a number of runtime conventions. It should
 * probably live among the domain library headers, and will probably
 * soon move there.
 */

/* Local Variables: */
/* comment-column:34 */


/* Conventions assumed by the runtime. This is the state of the world
 * on entry to main(). See Programmers Guide for explanation.
 * KR_VOID is defined in EROS/Invoke.h, as this is a kernel
 * constraint.
 */
#define KR_CONSTIT          1	  /* constituents node */
#define KR_RTBITS           2	  /* runtime components */
#define KR_CREATOR          3	  /* start key to process creator of self */
#define KR_SELF             4	  /* process key to self */
#define KR_BANK             5	  /* working storage */
#define KR_SCHED            6	  /* schedule of self (redundant,
				   * given process key, may get
				   * retired) */
#define KR_FIRST_FREE	    7	  /* first reg available reliably for
				     application use */
#define KR_APP(x)           (KR_FIRST_FREE+(x))
#define KR_ARG(x)           (28+(x))
#define KR_RETURN           31	  /* conventional return slot */

/* The following components are either
 *
 *     (a) universally used, or
 *     (b) a process cannot self-destruct without them.
 *
 * While several of these capabilities would not pass the discretion
 * test specified in the constructor logic, none of these capabilities
 * leak information, and all can safely be omitted from the discretion
 * test without hazarding confinement. These are known not to leak by
 * virtue of being implemented carefully in the system TSF.
 *
 * For each capability, the rationale for inclusion is also described.
 */

    /* A capability to the process creator who can created your
     * process. Without access to this process creator, a process
     * cannot perform the OC_Destroy operation, because it has no
     * means to demolish it's own process. Further, without its
     * process creator a process has no means to return a result from
     * the OC_Destroy operation. */
#define RKT_CREATOR        0

    /* Capability to the destroy space. The destroy space is a
     * primordial read-only address space that is used briefly during
     * the final stages of process self-destruction. When a process
     * has otherwise deallocated all of its resources, it places its
     * own address space and its desired return value in a well-known
     * capability register and proceeds with execution from the
     * destroy space. The destroy space tears down the process address
     * space and in turn invokes the process creator on behalf of the
     * dying process to arrange for the process itself to be destroyed
     * and for the final result code to be returned to the
     * requestor. */
#define RKT_DESTROY_SPACE  1

    /* Space Bank verifier. Used to determine whether a capability
     * that alleges to be a valid space bank capability is in fact a
     * valid space bank capability. Since the space bank is provided
     * by client code, a proprietary program must have a means to
     * determine that it is a legitemate space bank, lest the client
     * learn proprietary information by hijacking the service's source
     * of storage.
     *
     * Note that this may not be the "real" space bank verifier if
     * your object was created by an unorthodox constructor (e.g. one
     * that was using an alternative space bank implementation). The
     * contract is that this is the same space bank verifier used by
     * your constructor to determine if it was safe to instantiate
     * you.
     */
#define RKT_SB_VERIFIER    2

    /* Constructor verifier. Used to determine whether a capability
     * that alleges to be a constructor is in fact a constructor.
     * As constructors invoked by a service might be provided by a
     * client in a dynamic binding situation, a proprietary program
     * must have a means to determine that it is a legitemate space
     * bank, lest the client learn proprietary information by
     * hijacking some sub-object used by the service.
     *
     * Note that this may not be the "real" constructor verifier if
     * your object was created by an unorthodox constructor. The
     * contract is that this is the same constructor verifier used by
     * your constructor to determine if your constiuents were
     * discreet. */
#define RKT_CNS_VERIFIER   3

    /* Audit log capability. The audit log capability provides access
     * to a rate-limited logging mechanism (an OStream). Every process
     * received a uniquely distinguished audit logging capability.
     */
#define RKT_AUDIT_LOG      4

#endif /* __RUNTIME_H__ */
