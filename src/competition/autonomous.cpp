#include "competition/autonomous.h"
#include "automation.h"
#include "core.h"
#include "robot-config.h"
#include "vision.h"
#include <functional>

#define FWD vex::directionType::fwd
#define REV vex::directionType::rev

#define SIDE RED

enum Side { LEFT, RIGHT };

class WingCmd : public AutoCommand {
public:
  WingCmd(Side s, bool deploy_down) : s(s), deploy_down(deploy_down) {}

  bool run() override {
    if (s == LEFT) {
      if (deploy_down)
        left_wing.set(1);
      else
        left_wing.set(0);
    } else {
      if (deploy_down)
        right_wing.set(1);
      else
        right_wing.set(0);
    }
    return true;
  }

  Side s;
  bool deploy_down;
};

// Hacky solution, rotate X degrees every time it almost crosses the line
static std::atomic<int> angle_offset(90);

class IsCrossingYValCondition : public Condition {
public:
  IsCrossingYValCondition(int y_val) : y_val(y_val) {}
  bool test() override { 
    pose_t gps_pose;
    if(SIDE == RED)
    {
      gps_pose.x = gps_sensor.xPosition(distanceUnits::in) + 72;
      gps_pose.y = gps_sensor.yPosition(distanceUnits::in) + 72;
      gps_pose.rot = gps_sensor.heading(rotationUnits::deg);
    } else
    {
      gps_pose.x = 72 - gps_sensor.xPosition(distanceUnits::in);
      gps_pose.y = 72 - gps_sensor.yPosition(distanceUnits::in);
      gps_pose.rot = gps_sensor.heading(rotationUnits::deg);
    }
    if(gps_pose.y > y_val)
      angle_offset += 15;

    return gps_pose.y > y_val;
  }

private:
  int y_val;
};

/**
 * Main entrypoint for the autonomous period
 */
void autonomous() {
  while (imu.isCalibrating() || gps_sensor.isCalibrating()) {
    vexDelay(100);
  }
  awp_auto();
  // skills();
}

bool light_on() {
  vision_light.set(true);
  return true;
}

bool light_off() {
  vision_light.set(false);
  return true;
}

class DebugCommand : public AutoCommand {
public:
  bool run() override {
    drive_sys.stop();
    cata_sys.send_command(CataSys::Command::StopIntake);
    cata_sys.send_command(CataSys::Command::ToggleCata);
    vision_light.set(false);
    con.ButtonRight.pressed([](){
      vision_light.set(!vision_light.value());
    });
    pose_t pos = odom.get_position();
    printf("ODO X: %.2f, Y: %.2f, R:%.2f, ", pos.x, pos.y, pos.rot);
    while (true) {
      cata_sys.send_command(CataSys::Command::StopIntake);
      double f = con.Axis3.position() / 200.0;
      double s = con.Axis1.position() / 200.0;
      drive_sys.drive_arcade(f, s, 1, TankDrive::BrakeType::None);
      pose_t pos = odom.get_position();
      printf("ODO X: %.2f, Y: %.2f, R:%.2f\n", pos.x, pos.y, pos.rot);
      if (SIDE == RED)
      {
        printf("GPS X: %.2f, Y: %.2f, R: %.2f Q: %d\n",
          gps_sensor.xPosition(distanceUnits::in)+72,
          gps_sensor.yPosition(distanceUnits::in)+72,
          gps_sensor.heading(), gps_sensor.quality());
      } else
      {
        printf("GPS X: %.2f, Y: %.2f, R: %.2f Q: %d\n",
          72 - gps_sensor.xPosition(distanceUnits::in),
          72 - gps_sensor.yPosition(distanceUnits::in),
          gps_sensor.heading(), gps_sensor.quality());
      }
      
      cam.takeSnapshot(TRIBALL);
      printf(
        "X: %d, Y: %d, A: %d, Ratio: %f\n", cam.largestObject.centerX, cam.largestObject.centerY,
        cam.largestObject.width * cam.largestObject.height,
        (double)cam.largestObject.width / (double)cam.largestObject.height
      );
      vexDelay(100);
    }
    return false;
  }
};

void awp_auto() {

  static std::atomic<bool> end_vision_scan(false);
  // clang-format off
  DebugCommand *tempend = new DebugCommand();

  CommandController cmd{
    // ================ INIT ================
    odom.SetPositionCmd({.x=22, .y=21, .rot=225}),
    new DelayCommand(500),

    // ================ ALLIANCE TRIBALL 1 ================
    // Grab triball
    cata_sys.IntakeToHold(),
    drive_sys.DriveForwardCmd(drive_pid, 8, FWD, 0.2)->withCancelCondition(drive_sys.DriveStalledCondition(0.2)),
    cata_sys.WaitForHold(),

    // turn & try to score matchloads
    drive_sys.DriveForwardCmd(4, REV),
    drive_sys.TurnToHeadingCmd(140),
    new Async (new InOrder{
      new WaitUntilCondition(new FunctionCondition([](){ return odom.get_position().x > 75;})),
      new WingCmd(RIGHT, true),
      new WaitUntilCondition(new FunctionCondition([](){ return odom.get_position().x > 95;})),
      new WingCmd(LEFT, true),
      new WaitUntilCondition(new FunctionCondition([](){ return odom.get_position().y > 30;})),
      new WingCmd(LEFT, false),

    }),
    drive_sys.PurePursuitCmd(drive_pid, PurePursuit::Path({
      {.x=23, .y=21},
      {.x=27, .y=16},
      {.x=42, .y=14},
      {.x=94, .y=16},
      {.x=119, .y=22},
      {.x=120, .y=25},
      {.x=127, .y=29},
      {.x=126, .y=36} // {.x=129, .y=36},
    }, 8), REV, 0.6),
    // aim & push
    drive_sys.TurnDegreesCmd(-15),
    drive_sys.TurnToHeadingCmd(250),
    drive_sys.DriveForwardCmd(drive_pid, 100, REV, 1)->withTimeout(0.5),
    drive_sys.DriveForwardCmd(12, FWD)->withTimeout(1),
    new WingCmd(RIGHT, false),

    // Turn & score alliance triball
    drive_sys.TurnToHeadingCmd(135),
    drive_sys.TurnToHeadingCmd(77),
    cata_sys.Unintake(),
    new DelayCommand(300),
    drive_sys.DriveForwardCmd(drive_pid, 18, FWD, 0.9)->withCancelCondition(drive_sys.DriveStalledCondition(0.2)),
    cata_sys.StopIntake(),

    // Reverse & localize
    drive_sys.DriveForwardCmd(drive_pid, 8, REV, 0.9)->withCancelCondition(drive_sys.DriveStalledCondition(0.2)),
    new GPSLocalizeCommand(SIDE), // 136, 36 ish

    // ================ GRN TRIBALL 1 ================
    // Drive to position
    drive_sys.TurnToHeadingCmd(167),
    drive_sys.DriveToPointCmd({.x=104, .y=43}, FWD),
    drive_sys.TurnToHeadingCmd(90),

    (new RepeatUntil({
      // Scan for triballs (turn left slowly & search with camera)
      // Filter is important!
      new FunctionCommand(light_on),
      new FunctionCommand([](){
        const vision_filter_s filter = {
          .min_area = 3800,
          .max_area = 1000000,
          .aspect_low = 0.5,
          .aspect_high = 2.0,

          .min_x = 0,
          .max_x = 320,
          .min_y = 0,
          .max_y = 240,
        };
        auto objs = vision_run_filter(TRIBALL, filter);
        drive_sys.drive_tank_raw(-0.35, 0.35);

        // After reaching a max heading, stop scanning
        if(odom.get_position().rot > 225)
        {
          end_vision_scan = true;
          return true;
        }

        // Start tracking if objects are found, else keep scanning
        printf("num: %d, evs: %d\n", objs.size(), (bool)end_vision_scan);
        return objs.size() > 0;
      }),

      new Branch(
        new FunctionCondition([]() -> bool { printf("testing\n");return end_vision_scan;}),
        new InOrder{ // FALSE - do NOT end vision scan, start tracking the ball
          cata_sys.IntakeToHold(),
          (new VisionTrackTriballCommand())->withCancelCondition(new IsCrossingYValCondition(71)),
          new FunctionCommand(light_off),
          drive_sys.DriveToPointCmd({102, 58}, REV),
          drive_sys.TurnToHeadingCmd(0),

          cata_sys.Unintake(),
          drive_sys.DriveForwardCmd(drive_pid, 100, FWD, 0.5)->withTimeout(1),
          cata_sys.StopIntake(),
          drive_sys.DriveForwardCmd(12, REV),
          // Hacky solution to turn a different amount each time we almost cross
          new FunctionCommand([](){
            CommandController({drive_sys.TurnToHeadingCmd(angle_offset)}).run();
            return true;
          }),
        },
        new InOrder{ // TRUE - END the vision scan, exit the repeat.
        // Do nothing here
        }),
      
    }, new IfTimePassed(45)))->withCancelCondition(new FunctionCondition([]()->bool{ return end_vision_scan; })),
    new FunctionCommand(light_off),
    // ================ GO TO BAR AWP ================
    // drive_sys.TurnToHeadingCmd(180),
    // new GPSLocalizeCommand(SIDE),
    drive_sys.TurnToHeadingCmd(225),
    // drive_sys.DriveToPointCmd({.x=89, .y=52}, FWD),
    tempend,
    cata_sys.Unintake(),
    drive_sys.DriveForwardCmd(drive_pid, 144, FWD, 0.3)->withCancelCondition(drive_sys.DriveStalledCondition(0.5)),
    cata_sys.StopIntake(),
    drive_sys.TurnToHeadingCmd(drive_pid, 180, 0.2),

    
  };

  cmd.run();
  // clang-format on
}

FunctionCondition *is_in_cata() {
  return new FunctionCondition([]() { return cata_watcher.isNearObject(); });
}

void skills() {
  // clang-format off
  auto tempend = new DebugCommand();

  CommandController cmd {
    odom.SetPositionCmd({.x = 19, .y = 117, .rot = 135}), // GPS says starting pt is
    (new RepeatUntil(InOrder {

      // Matchloading!
      cata_sys.IntakeFully(),
      // Push against bar slowly & wait for triball to load
      new FunctionCommand([]() {
        drive_sys.drive_tank(0.1, 0.1);
        return true;
      }),

      // Up against the wall, reset odometry
      cata_sys.WaitForIntake()->withTimeout(2), odom.SetPositionCmd({.x = 15, .y = 121, .rot = 135}),

      new FunctionCommand([]() {
        vex::task([]() {
          vexDelay(600);
          cata_sys.send_command(CataSys::Command::StartFiring);
          return 0;
        });

        return true;
      }),

      // Drive to firing position
      drive_sys.PurePursuitCmd(drive_pid, PurePursuit::Path({
          {.x = 15, .y = 121},
          {.x = 17, .y = 120},
          {.x = 19, .y = 120},
          {.x = 27, .y = 120},
        }, 4), REV, 0.3),

      drive_sys.DriveForwardCmd(6.75, FWD)->withTimeout(3), drive_sys.TurnToHeadingCmd(135)->withTimeout(3),
      drive_sys.DriveForwardCmd(6, FWD)->withTimeout(3),

      // Start intake & drive back to loading area
      cata_sys.IntakeFully(),

      // Done, go back to beginning of matchloading

    },

    new IfTimePassed(42))),
    cata_sys.Fire(),

    // Deploy wing while driving after crossing X value
    // May not be needed for side goal
    new Async(new InOrder{
      new WaitUntilCondition(new FunctionCondition([](){ return odom.get_position().x > 64;})),
      new WingCmd(LEFT, true),
      new WaitUntilCondition(new FunctionCondition([](){ return odom.get_position().x > 72;})),
      new WingCmd(RIGHT, true),
      new WaitUntilCondition(new FunctionCondition([](){ return odom.get_position().y < 118;})),
      new WingCmd(RIGHT, false),
      // new WingCmd(LEFT, false)
    }),

    // Drive SLOWLY under bar (don't push just yet)
    drive_sys.PurePursuitCmd(drive_pid, PurePursuit::Path({
        {.x = 15, .y = 122},
        {.x = 21, .y = 124},
        {.x = 29, .y = 135},
        {.x = 41, .y = 135},
        {.x = 71, .y = 135},
        {.x = 98, .y = 127},
        {.x = 105, .y = 123},
        {.x = 120, .y = 120},
        {.x = 130, .y = 110},
      }, 9), REV, 0.42),
      
    new WingCmd(RIGHT, false),
    drive_sys.TurnToHeadingCmd(90),

    // Ram, back up & ram again
    drive_sys.DriveForwardCmd(drive_pid, 24, REV, 1.0)->withTimeout(1),
    
    odom.SetPositionCmd({.x = 132, .y = 102, .rot = 90}),

    drive_sys.DriveForwardCmd(drive_pid, 20, FWD, 0.5)->withTimeout(1),

    drive_sys.TurnToHeadingCmd(110),

    drive_sys.DriveForwardCmd(drive_pid, 24, REV, 1)->withTimeout(1),
    
    odom.SetPositionCmd({.x = 132, .y = 102, .rot = 105}),

    drive_sys.DriveForwardCmd(drive_pid, 20, FWD, 0.5)->withTimeout(1),

    drive_sys.DriveForwardCmd(drive_pid, 24, REV, 1)->withTimeout(1),

    odom.SetPositionCmd({.x = 132, .y = 102, .rot = 105}),

    drive_sys.DriveForwardCmd(drive_pid, 20, FWD, 0.5)->withTimeout(1),

    drive_sys.TurnToHeadingCmd(90),

    drive_sys.DriveForwardCmd(drive_pid, 24, REV, 1)->withTimeout(1),

    odom.SetPositionCmd({.x = 132, .y = 102, .rot = 90}),

    drive_sys.DriveForwardCmd(drive_pid, 20, FWD, 0.5)->withTimeout(1),


    

    // TODO Set gps to red
    // new GPSLocalizeCommand(BLUE),
    //drive_sys.DriveForwardCmd(9, FWD),
    // drive_sys.PurePursuitCmd(drive_pid, PurePursuit::Path({
    //   {.x = 132, .y = 111.6},
    //   {.x = 128.5, .y = 118.25},
    //   {.x = 119, .y = 126.7},
    //   {.x = 107, .y = 129.73},
    //   {.x = 94.25, .y = 129.65},
    // }, 4), FWD, .5),
    // drive_sys.DriveToPointCmd({.x = 112, .y = 126}, FWD, .4),
    // drive_sys.TurnToHeadingCmd(180),
    // drive_sys.DriveToPointCmd({.x = 96, .y = 126}, FWD, .4),
    // new WingCmd(LEFT, true),
    //new DebugCommand(),
    // drive_sys.PurePursuitCmd(drive_pid, PurePursuit::Path({
    //   {.x = 123, .y = 120},
    //   {.x = 126, .y = 122},
    //   {.x = 129, .y = 120},
    //   {.x = 132, .y = 113},
    //   {.x = 136, .y = 108},
    //   {.x = 130, .y = 98},
    // }, 4), REV, .1),
    
    
    // odom.SetPositionCmd({.x = 96, .y = 132, .rot = 180}),
    // drive_sys.DriveToPointCmd({.x = 100, .y = 132}, REV, .4),
    // drive_sys.TurnToHeadingCmd(150),
    // drive_sys.DriveToPointCmd({.x = 123, .y = 118}, REV, .4),
    // drive_sys.TurnToHeadingCmd(110),
    // drive_sys.DriveToPointCmd({.x = 126, .y = 111}, REV, .4),
    // new DebugCommand(),
    // drive_sys.TurnToHeadingCmd(90),
    // //drive_sys.TurnToHeadingCmd(90),
    // new WingCmd(LEFT, false),
    // drive_sys.DriveForwardCmd(18, FWD),
    // drive_sys.DriveForwardCmd(drive_pid, 24, REV, 0.8)->withTimeout(1),
    // drive_sys.DriveForwardCmd(12, FWD),

    // tempend,

    // Backup & localize
    // drive_sys.PurePursuitCmd(drive_pid, PurePursuit::Path({
    //     {.x = 0, .y = 0},
    //     {.x = 0, .y = 0},
    //     {.x = 0, .y = 0},
    //   }, 8), FWD, 0.4),
    // drive_sys.TurnToHeadingCmd(45),
    // new GPSLocalizeCommand(RED),
  };

  cmd.run();
  drive_sys.stop();
  // clang-format on
}