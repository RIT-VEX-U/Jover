#include "competition/autonomous.h"
#include "robot-config.h"
#include "core.h"
#include "automation.h"
#include <functional>

#define FWD vex::directionType::fwd
#define REV vex::directionType::rev

#define RED

enum Side
{
    LEFT, RIGHT
};

class WingCmd : public AutoCommand
{
    public:
    WingCmd(Side s, bool deploy_down) : s(s), deploy_down(deploy_down)
    { }

    bool run() override
    { 
        if(s == LEFT)
        {
            if(deploy_down)
                left_wing.set(1);
            else
                left_wing.set(0);
        } else
        {
            if(deploy_down)
                right_wing.set(1);
            else
                right_wing.set(0);
        }
        return true; 
    }

    Side s;
    bool deploy_down;
};

/**
 * Main entrypoint for the autonomous period
 */


void autonomous()
{
    while(imu.isCalibrating() || gps_sensor.isCalibrating()){vexDelay(100);}
    scoreAutoAWP();
}

FunctionCommand* gps_reset()
{
    return new FunctionCommand([](){
        while(gps_sensor.xPosition() == 0.0 && gps_sensor.yPosition() == 0.0) {vexDelay(20);}

        pose_t orig = odom.get_position();
        static timer t;
        t.reset();
        double x = orig.x, y= orig.y, rot = orig.rot;
        int itr = 0;
        while(t.time(sec) < 1)
        {
            if(gps_sensor.quality() > 99)
            {
                x += gps_sensor.xPosition(distanceUnits::in)+72;
                y += gps_sensor.yPosition(distanceUnits::in)+72;
                rot = gps_sensor.heading();
                itr++;
            }
        }

        if(itr > 0)
        {
            x = x / itr;
            y = y / itr;
        }

        odom.set_position({
            #ifdef RED
                .x = x,
                .y = y,
                .rot = rot
            #else
                .x = 144 - x,
                .y = 144 - y,
                .rot = rot - 180
            #endif
        });

        return true;
    });
}

class DebugCommand : public AutoCommand
{
    public:

    bool run() override
    {
        drive_sys.stop();
        cata_sys.send_command(CataSys::Command::StopIntake);
        while(true)
        {
            cata_sys.send_command(CataSys::Command::StopIntake);
            double f = con.Axis3.position() / 200.0;
            double s = con.Axis1.position() / 200.0;
            drive_sys.drive_arcade(f, s, 1, TankDrive::BrakeType::None);
            pose_t pos = odom.get_position();
            printf("X: %.2f, Y: %.2f, R:%.2f\n", pos.x, pos.y, pos.rot);
            printf("GPS X: %.2f, Y: %.2f, R: %.2f Q: %d\n", 
                gps_sensor.xPosition(distanceUnits::in)+72, 
                gps_sensor.yPosition(distanceUnits::in)+72, 
                gps_sensor.heading(), gps_sensor.quality());
            vexDelay(100);
        }
        return false;
    }
};

void scoreAutoAWP()
{
    DebugCommand *tempend = new DebugCommand();

    CommandController cmd{
        odom.SetPositionCmd({.x=54, .y=15, .rot=0}),
        // Collect Ball (after delay)
        new Async(new InOrder{
            new WaitUntilCondition(new FunctionCondition([](){
                return odom.get_position().x > 64;
            })),
            cata_sys.IntakeToHold(),
        }),
    
        // Drive to goal
        drive_sys.PurePursuitCmd(PurePursuit::Path({
            {.x=54, .y=15},
            {.x=97, .y=17},
            {.x=120, .y=28},
            {.x=127, .y=38},
        }, 8), directionType::fwd, 0.4),
        drive_sys.TurnToHeadingCmd(90, 0.4),
        cata_sys.Outtake(),
        new DelayCommand(500),
        drive_sys.DriveForwardCmd(10, FWD, 0.8)->withTimeout(1),
        cata_sys.StopIntake(),
        gps_reset(), // 126, 39, 90 ish
        // tempend,

        // Back up & get alliance ball
        // drive_sys.DriveToPointCmd({.x=117, .y=22.6}, REV, 0.4),
        drive_sys.PurePursuitCmd(PurePursuit::Path({
            {.x=126, .y=39},
            {.x=125, .y=32},
            {.x=114, .y=26}
        }, 8), REV, 0.4),
        drive_sys.TurnToHeadingCmd(310, 0.4),
        cata_sys.IntakeToHold(),
        drive_sys.DriveForwardCmd(12, FWD, 0.4)->withTimeout(.7),
        cata_sys.WaitForIntake()->withTimeout(3),
        drive_sys.DriveForwardCmd(4, REV, 0.4),
        
        // Score alliance ball
        drive_sys.TurnToHeadingCmd(61, 0.4),
        drive_sys.DriveToPointCmd({.x=124, .y=35}, FWD, .4),
        drive_sys.TurnToHeadingCmd(90, 0.4),
        cata_sys.Outtake(),
        new DelayCommand(500),
        drive_sys.DriveForwardCmd(10, FWD, 0.8)->withTimeout(1),
        cata_sys.StopIntake(),
        drive_sys.DriveForwardCmd(3, REV, 0.4)->withTimeout(1),
        gps_reset(),

        // Touch bar
        drive_sys.DriveToPointCmd({.x=125, .y=29}, REV, .4),
        drive_sys.TurnToHeadingCmd(216, 0.4),
        drive_sys.PurePursuitCmd(PurePursuit::Path({
            {.x=125, .y=29},
            {.x=105, .y=14},
            {.x=86, .y=13}
        }, 8), FWD, 0.4),
        drive_sys.TurnToHeadingCmd(141, 0.4),
        drive_sys.DriveForwardCmd(12, FWD, 0.2)->withTimeout(2),
    };

    cmd.run();
    
}

AutoCommand *scoreFromMiddle()
{
    return new InOrder{
        // grab from bar
        drive_sys.TurnToHeadingCmd(0, 0.4),
        cata_sys.IntakeToHold(), 
        (new VisionTrackTriballCommand())->withTimeout(3),
        drive_sys.DriveForwardCmd(0, REV, 0.4),

        // Score in goal
        drive_sys.TurnToHeadingCmd(0, 0.4),
        drive_sys.DriveToPointCmd({.x=0, .y=0}, REV, 0.4),
        drive_sys.TurnToHeadingCmd(0, 0.4),
        cata_sys.Outtake(),
        drive_sys.DriveForwardCmd(0, FWD, 0.6)->withTimeout(1), 

        // Reverse & localize
        drive_sys.DriveForwardCmd(0, REV, 0.4),
        gps_reset(),

        // drive to a neutral position
        drive_sys.TurnToHeadingCmd(0, 0.4),
        drive_sys.DriveToPointCmd({.x=0, .y=0}, REV, 0.4),
        drive_sys.TurnToHeadingCmd(0, 0.4),

    };
}

void scoreAutoFull()
{
    DebugCommand *tempend = new DebugCommand();

    // Create an object command that can be called at any point
    InOrder *touch_bar = new InOrder{
        drive_sys.TurnToHeadingCmd(0, 0.4),
        // Grab odometry's current position to init pure pursuit
        new FunctionCommand([](){
            pose_t robot_pos = odom.get_position();
            CommandController cmd {
                drive_sys.PurePursuitCmd(PurePursuit::Path({
                    {robot_pos.x, robot_pos.y},
                    {0, 0},
                    {0, 0}
                }, 8), FWD, 0.4)
            };
            cmd.run();
            return true;
        }),

        // After getting in position, turn & touch the bar
        drive_sys.TurnToHeadingCmd(0, 0.3),
        drive_sys.DriveForwardCmd(0, FWD, 0.3),
    };

    CommandController cmd{
        odom.SetPositionCmd({.x=54, .y=15, .rot=0}),
        // Collect Ball (after delay)
        new Async(new InOrder{
            new WaitUntilCondition(new FunctionCondition([](){
                return odom.get_position().x > 64;
            })),
            cata_sys.IntakeToHold(),
        }),
    
        // Drive to goal
        drive_sys.PurePursuitCmd(PurePursuit::Path({
            {.x=54, .y=15},
            {.x=97, .y=17},
            {.x=120, .y=28},
            {.x=127, .y=38},
        }, 8), directionType::fwd, 0.4),
        drive_sys.TurnToHeadingCmd(90, 0.4),
        cata_sys.Outtake(),
        new DelayCommand(500),
        drive_sys.DriveForwardCmd(10, FWD, 0.8)->withTimeout(1),
        cata_sys.StopIntake(),
        gps_reset(), // 126, 39, 90 ish

        // Back up & get alliance ball
        // TODO use vision? prob not since its not green
        // drive_sys.DriveToPointCmd({.x=117, .y=22.6}, REV, 0.4),
        drive_sys.PurePursuitCmd(PurePursuit::Path({
            {.x=126, .y=39},
            {.x=125, .y=32},
            {.x=114, .y=26}
        }, 8), REV, 0.4),
        drive_sys.TurnToHeadingCmd(310, 0.4),
        cata_sys.IntakeToHold(),
        drive_sys.DriveForwardCmd(12, FWD, 0.4)->withTimeout(.7),
        cata_sys.WaitForIntake()->withTimeout(3),
        drive_sys.DriveForwardCmd(4, REV, 0.4),
        
        // Score alliance ball
        drive_sys.TurnToHeadingCmd(61, 0.4),
        drive_sys.DriveToPointCmd({.x=124, .y=35}, FWD, .4),
        drive_sys.TurnToHeadingCmd(90, 0.4),
        cata_sys.Outtake(),
        new DelayCommand(500),
        drive_sys.DriveForwardCmd(10, FWD, 0.8)->withTimeout(1),
        cata_sys.StopIntake(),
        drive_sys.DriveForwardCmd(8, REV, 0.4)->withTimeout(1),
        gps_reset(),

        tempend,

        // BEGIN UNTESTED

        // Drive into position
        drive_sys.TurnToHeadingCmd(0, 0.4),
        drive_sys.PurePursuitCmd(PurePursuit::Path({
            {.x=0, .y=0},
            {.x=0, .y=0},
            {.x=0, .y=0},
        }, 8), FWD, 0.4),
        
        // grab from center (closest to goal)
        // TODO make sure we don't cross the line! (Async command?)
        drive_sys.TurnToHeadingCmd(0, 0.4),
        new Branch {
            new VisionObjectExists(), // Maybe add filter for left/right limits?
            new InOrder { // Object exists, go get 'em!
                new VisionTrackTriballCommand(),
                // Back up & score
                drive_sys.DriveToPointCmd({.x=0, .y=0}, REV, 0.4),
                drive_sys.TurnToHeadingCmd(0, 0.4),
                drive_sys.DriveToPointCmd({.x=0, .y=0}, FWD, 0.4),
                cata_sys.Outtake(),
                drive_sys.DriveForwardCmd(0, FWD, 0.6)->withTimeout(1),
                cata_sys.StopIntake()

            },
            new InOrder{ // Object doesn't exist, big sad (turn & continue on)
                drive_sys.TurnToHeadingCmd(0, 0.4)
            }
        },

        // Score

        // grab from center (furthest from goal)

        // Score

        // grab from bar (closest)

    };

    cmd.run();
}

void skills()
{
    FunctionCommand *intakeToCata = new FunctionCommand([](){
        drive_sys.drive_tank(0.15, 0.15);
        // Run intake in, periodically push out in case it's caught.
        // static vex::timer intake_tmr;
        // if(intake_tmr.time(sec) > 1)
        //     cata_sys.send_command(CataSys::Command::IntakeOut);
        // else if( intake_tmr.time(sec) > 1.5)
        //     intake_tmr.reset();
        // else
        //     cata_sys.send_command(CataSys::Command::IntakeIn);

        // Only return when the ball is in the bot
        return cata_watcher.isNearObject();
    });

    FunctionCommand *tempend = new FunctionCommand([](){
        drive_sys.stop();
        cata_sys.send_command(CataSys::Command::StopIntake);
        while(true)
        {
            cata_sys.send_command(CataSys::Command::StopIntake);
            double f = con.Axis3.position() / 200.0;
            double s = con.Axis1.position() / 200.0;
            drive_sys.drive_arcade(f, s, 1, TankDrive::BrakeType::None);
            pose_t pos = odom.get_position();
            printf("X: %.2f, Y: %.2f, R:%.2f\n", pos.x, pos.y, pos.rot);
            vexDelay(100);
        }
        return false;
    });

    CommandController cmd{
        odom.SetPositionCmd({.x=18, .y=126, .rot=135}),
        
        // 1 - Turn and shoot preload
        drive_sys.TurnToHeadingCmd(160, .5),
        cata_sys.Fire(),
        new DelayCommand(800),

        // 2 - Turn to matchload zone & begin matchloading
        drive_sys.TurnToHeadingCmd(135, .5),
        cata_sys.IntakeFully(),
        drive_sys.DriveToPointCmd({.x=13.8, .y=130.3}, FWD, 0.5),

        new FunctionCommand([](){cata_sys.send_command(CataSys::Command::StopFiring); return true;}),
        cata_sys.IntakeFully(),
        // Matchloading phase
        (new RepeatUntil(InOrder{
            intakeToCata->withTimeout(3),
            // cata_sys.Fire(),
            drive_sys.DriveToPointCmd({.x=18, .y=126}, REV, 0.5)->withTimeout(1),
            drive_sys.TurnToHeadingCmd(160, 0.5)->withTimeout(1),
            cata_sys.Fire(),
            new DelayCommand(300),
            drive_sys.TurnToHeadingCmd(135, 130.3)->withTimeout(1),
            new FunctionCommand([](){cata_sys.send_command(CataSys::Command::StopFiring); return true;}),
            cata_sys.IntakeFully(),
            drive_sys.DriveToPointCmd({.x=13, .y=130}, FWD, 0.5)->withTimeout(1),
            
        }, new FunctionCondition([](){return false;})))
            ->withTimeout(30),

        // Last preload
        drive_sys.DriveToPointCmd({.x=18, .y=126}, FWD, 0.5),
        drive_sys.TurnToHeadingCmd(160, 0.5),
        cata_sys.Fire(),
        new DelayCommand(300),
        cata_sys.StopIntake(),
        drive_sys.TurnToHeadingCmd(204, 0.5),

        // Drive through "The Passage" & push into side of goal
        // Push into side of goal
        // new Parallel{
        //     new InOrder{
        //         new WaitUntilCondition(new FunctionCondition([](){
        //             return odom.get_position().x > 100;
        //         })),
        //         new WingCmd(RIGHT, true)
        //     },
        cata_sys.StopIntake(),
        drive_sys.PurePursuitCmd(PurePursuit::Path({
            {.x=19, .y=133},
            {.x=40, .y=136},
            {.x=92, .y=136},
            {.x=105, .y=128},
            {.x=116, .y=117},
            {.x=120, .y=98},
            }, 8), REV, .5)->withTimeout(4),
        // },


        // FULL SPEED AHEAD
        drive_sys.DriveForwardCmd(18, FWD, 0.5),
        drive_sys.DriveForwardCmd(48, REV, 0.8)->withTimeout(1),

        // drive_sys.PurePursuitCmd(PurePursuit::Path({
        //     {.x=0, .y=0},
        // }, 8), FWD, 0.3),

        //Wall Align
        drive_sys.TurnToHeadingCmd(90, 0.5),
        drive_sys.DriveForwardCmd(100, FWD, 0.25)->withTimeout(3),
        cata_sys.Outtake(),
        drive_sys.TurnDegreesCmd(90, .5)->withTimeout(1),
        drive_sys.TurnDegreesCmd(-90, .5)->withTimeout(1),
        drive_sys.DriveForwardCmd(100, FWD, 0.25)->withTimeout(2),
        cata_sys.StopIntake(),

        odom.SetPositionCmd({138, 138, 45}),
        drive_sys.DriveToPointCmd({.x=127, .y=127}, REV, .5),
        new WingCmd(LEFT, true),

        // Curve to final position
        drive_sys.PurePursuitCmd(PurePursuit::Path({
            {.x=127, .y=127},
            {.x=118, .y=113},
            {.x=127, .y=98},
        }, 12.0), REV, 0.4),
        drive_sys.TurnToHeadingCmd(180, .5)->withTimeout(2),

        //  FULL SPEED AHEAD
        drive_sys.DriveForwardCmd(18, FWD, 0.5),
        drive_sys.DriveForwardCmd(48, REV, 0.8)->withTimeout(1),

        // Get out of the way & end
        new WingCmd(LEFT, false),
        drive_sys.TurnToHeadingCmd(129, .5),
        drive_sys.DriveForwardCmd(24, FWD, 0.5),
    };

    cmd.run();

    drive_sys.stop();
}