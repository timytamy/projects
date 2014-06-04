/*======================================================================

    A driver for PCMCIA digimedia dmx512 devices

    digimedia_cs.h

    The contents of this file are subject to the Mozilla Public
    License Version 1.1 (the "License"); you may not use this file
    except in compliance with the License. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

    Software distributed under the License is distributed on an "AS
    IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
    implied. See the License for the specific language governing
    rights and limitations under the License.

    The initial developer of the original code is David A. Hinds
    <dahinds@users.sourceforge.net>.  Portions created by David A. Hinds
    are Copyright (C) 1999 David A. Hinds.  All Rights Reserved.

    Alternatively, the contents of this file may be used under the
    terms of the GNU General Public License version 2 (the "GPL"), in which
    case the provisions of the GPL are applicable instead of the
    above.  If you wish to allow the use of your version of this file
    only under the terms of the GPL and not to allow others to use
    your version of this file under the MPL, indicate your decision
    by deleting the provisions above and replace them with the notice
    and other provisions required by the GPL.  If you do not delete
    the provisions above, a recipient may use your version of this
    file under either the MPL or the GPL.

======================================================================*/

#ifndef __DGM_CS_H__
#define __DGM_CS_H__

#include "dgm.h"

#define DGM_CS_PRODID_1      "Digimedia MLS"
#define DGM_CS_PRODID_2      "Dual link DMX-512 PC Card"
#define DGM_CS_PRODID_3      "DMXPCC"
#define DGM_CS_PRODID_4      "V2.08"

#define DGM_CS_BOARD_TYPE    0
#define DGM_CS_BOARD_INFO   "Digimedia - Soundlight DMX card 2512 PCMCIA"
#define DGM_CS_MEMORY_OFFSET (0x0800)
#define DGM_CS_MEMORY_REGION 1

#endif
