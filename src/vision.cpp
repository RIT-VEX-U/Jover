/*vex-vision-config:begin*/
#include "vex.h"
vex::vision::signature TRIBALL = vex::vision::signature (1, -5503, -4555, -5028, -5795, -4803, -5298, 5.7, 0);
vex::vision::signature SIG_2 = vex::vision::signature (2, 0, 0, 0, 0, 0, 0, 2.5, 0);
vex::vision::signature SIG_3 = vex::vision::signature (3, 0, 0, 0, 0, 0, 0, 2.5, 0);
vex::vision::signature SIG_4 = vex::vision::signature (4, 0, 0, 0, 0, 0, 0, 2.5, 0);
vex::vision::signature SIG_5 = vex::vision::signature (5, 0, 0, 0, 0, 0, 0, 2.5, 0);
vex::vision::signature SIG_6 = vex::vision::signature (6, 0, 0, 0, 0, 0, 0, 2.5, 0);
vex::vision::signature SIG_7 = vex::vision::signature (7, 0, 0, 0, 0, 0, 0, 2.5, 0);
vex::vision cam = vex::vision (vex::PORT6, 70, TRIBALL, SIG_2, SIG_3, SIG_4, SIG_5, SIG_6, SIG_7);
/*vex-vision-config:end*/