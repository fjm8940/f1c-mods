#ifndef SC_RADIO_FILTER_H
#define SC_RADIO_FILTER_H

/* Exact native queue event IDs. 126 (GREEN) and sanction/SC messages stay
 * audible.  Filter only while this mod's caution still owns the flag. */
static int sc_radio_suppress(int eid,int caution_active) {
    if(!caution_active)return 0;
    switch(eid) {
    case 134: case 136: case 138: /* local yellow: sectors 1-3 */
    case 133: case 135: case 137: /* local yellow cleared */
    case 232:                       /* AI spun off in sector X; yellow warning */
    case 120: case 413:           /* player overtakes AI */
    case 41:  case 332:           /* player position praise */
        return 1;
    default:return 0;
    }
}
#endif
