// Live override: wetness_scale_up  (0x0059e450)  float=0.06
'use strict';
const P = ptr('0x0059e450');
const ORIG = '0000003f';
Memory.protect(P, 4, 'rwx');
P.writeFloat(0.06);
console.log('[patch] wetness_scale_up applied; original was ' + ORIG);
