#include "Robot.h" 
//#include <chrono>
//using namespace std::chrono;

void write2File(Vector3d* input, int size, string file_name="data"){
    ofstream output_file(file_name + ".csv");
    for(int i=0; i<size; i++){
         output_file << input[i](0) << " ,";
         output_file << input[i](1) << " ,";
         output_file << input[i](2) << " ,";
         output_file << "\n";
    }
    output_file.close();
}


Robot::Robot(ros::NodeHandle *nh, Controller robot_ctrl){

    trajGenServer_ = nh->advertiseService("/traj_gen", 
            &Robot::trajGenCallback, this);
    jntAngsServer_ = nh->advertiseService("/jnt_angs", 
            &Robot::jntAngsCallback, this);
    generalTrajServer_ = nh->advertiseService("/general_traj", 
            &Robot::generalTrajCallback, this);
    resetTrajServer_ = nh->advertiseService("/reset_traj",
            &Robot::resetTrajCallback, this);
    zmpDataPub_ = nh->advertise<geometry_msgs::Point>("/zmp_position", 100);
    comDataPub_ = nh->advertise<geometry_msgs::Twist>("/com_data", 100);
    xiDataPub_ = nh->advertise<geometry_msgs::Twist>("/xi_data", 100);
    
    // SURENA IV geometrical params
    
    thigh_ = 0.36;  // SR1: 0.3535, Surena4: 0.37, Surena5: 0.36
    shank_ = 0.35;     // SR1: 0.3, Surena4: 0.36, Surena5: 0.35
    torso_ = 0.0975;    // SR1: 0.09, Surena4: 0.115, Surena5: 0.0975
    double sole_x_front = 0.16;     // Surena4: ??, Surena5: 0.16
    double sole_y = 0.085;          // Surena4: ??, Surena5: 0.085
    double sole_x_back = 0.09;      // Surena4: ??, Surena5: 0.09
    double min_dist = 0.18;         // Surena4: ??, Surena5: 0.18

    mass_ = 55.7; // SR1: ?, Surena4: 48.3, Surena5: 55.7(Solid: 43.813)

    dataSize_ = 0;
    rSole_ << 0.0, -torso_, 0.0;
    lSole_ << 0.0, torso_, 0.0;       // might be better if these two are input argument of constructor
    isTrajAvailable_ = false;

    Vector3d a[12];
	Vector3d b[12];
	// Defining Joint Axis
	a[0] << 0.0, 0.0, 1.0;
	a[1] << 1.0, 0.0, 0.0;
	a[2] << 0.0, 1.0, 0.0;
	a[3] << 0.0, 1.0, 0.0;
	a[4] << 0.0, 1.0, 0.0;
	a[5] << 1.0, 0.0, 0.0;
	a[6] = a[0];
	a[7] = a[1];
	a[8] = a[2];
	a[9] = a[3];
	a[10] = a[4];
	a[11] = a[5];
	// Defining Joint Positions
	b[0] << 0.0, -torso_, 0.0;
	b[1] << 0.0, 0.0, 0.0;
	b[2] = b[1];
	b[3] << 0.0, 0.0, -thigh_;
	b[4] << 0.0, 0.0, -shank_;
	b[5] = b[1];
	b[6] = -b[0];
	b[7] = b[1];
	b[8] = b[2];
	b[9] = b[3];
	b[10] = b[4];
	b[11] = b[5];

    _Link* pelvis = new _Link(0, Vector3d::Ones(3), Vector3d(0, 0, thigh_ + shank_), 13.2473824912366, Matrix3d::Identity(3,3), Vector3d(0.0063, -0.00179, 0.306));
    Vector3d position(0.0, 0.0, 0.0);
	pelvis->initPose(position, Matrix3d::Identity(3, 3));
    links_[0] = pelvis;
    _Link* rHipY = new _Link(1, a[0], b[0], 3.1729, Matrix3d::Identity(3, 3), Vector3d(-0.07423, 0, 0.0213), links_[0]);
    links_[1] = rHipY;
    _Link* rHipR = new _Link(2, a[1], b[1], 2.44028233906507, Matrix3d::Identity(3, 3), Vector3d(0.000984969543886728, 0.0247755120757208, -0.00015880471566998), links_[1]);
    links_[2] = rHipR;
    _Link* rHipP = new _Link(3, a[2], b[2], 5.27157, Matrix3d::Identity(3, 3), Vector3d(-0.00086, 0.01229, -0.18794), links_[2]);
    links_[3] = rHipP;
    _Link* rKnee = new _Link(4, a[3], b[3], 2.23575, Matrix3d::Identity(3, 3), Vector3d(0.01649, 0.00164, -0.08690), links_[3]);
    links_[4] = rKnee;
    _Link* rAnkleP = new _Link(5, a[4], b[4], 0.18834, Matrix3d::Identity(3, 3), Vector3d(-0.03478, 0.00111, -0.00042), links_[4]);
    links_[5] = rAnkleP; 
    _Link* rAnkleR = new _Link(6, a[5], b[5], 1.80660460486529, Matrix3d::Identity(3, 3), Vector3d(0.0261782638901839, 0.000138793078492322, -0.040153711347177), links_[5]);
    links_[6] = rAnkleR; 

    _Link* lHipY = new _Link(7, a[6], b[6], 3.1707, Matrix3d::Identity(3, 3), Vector3d(-0.07423, 0, 0.0213), links_[0]);
    links_[7] = lHipY;
    _Link* lHipR = new _Link(8, a[7], b[7], 2.43805, Matrix3d::Identity(3, 3), Vector3d(0.00085, -0.02478, 0.00015), links_[7]);
    links_[8] = lHipR;
    _Link* lHipP = new _Link(9, a[8], b[8], 5.61487, Matrix3d::Identity(3, 3), Vector3d(-0.00086, -0.013, -0.18504), links_[8]);
    links_[9] = lHipP;
    _Link* lKnee = new _Link(10, a[9], b[9], 2.23418, Matrix3d::Identity(3, 3), Vector3d(0.01575 , -0.00162, -0.08673), links_[9]);
    links_[10] = lKnee;
    _Link* lAnkleP = new _Link(11, a[10], b[10], 0.18834, Matrix3d::Identity(3, 3), Vector3d(-0.003478, -0.00112, -0.00042), links_[10]);
    links_[11] = lAnkleP; 
    _Link* lAnkleR = new _Link(12, a[11], b[11], 1.8051, Matrix3d::Identity(3, 3), Vector3d(0.02617, 0.00013891, -0.040175), links_[11]);
    links_[12] = lAnkleR;

    onlineWalk_ = robot_ctrl;

    ankleColide_ = new Collision(sole_x_front, sole_y, sole_x_back, min_dist);
    estimator_ = new Estimator();

    bumpBiasSet_ = false;
    bumpBiasR_ = -56.0;
    bumpBiasL_ = -57.75;

    //cout << "Robot Object has been Created" << endl;
}

void Robot::spinOnline(int iter, double config[], double jnt_vel[], Vector3d torque_r, Vector3d torque_l, double f_r, double f_l, Vector3d gyro, Vector3d accelerometer, int bump_r[], int bump_l[], double* joint_angles, int& status){
    // update joint positions
    for (int i = 0; i < 13; i ++){
        links_[i]->update(config[i], jnt_vel[i], 0.0);
    }
    // Do the Forward Kinematics for Lower Limb
    // for(int i=0; i<12; i++){
    //     cout << config[i+1] << ',';
    // }
    links_[12]->FK();
    links_[6]->FK();    // update all raw values of sensors and link states
    updateState(config, torque_r, torque_l, f_r, f_l, gyro, accelerometer);
    MatrixXd lfoot(3,1);
    MatrixXd rfoot(3,1);
    Matrix3d attitude = MatrixXd::Identity(3,3);
    MatrixXd pelvis(3,1);
    lfoot << lAnklePos_[iter](0), lAnklePos_[iter](1), lAnklePos_[iter](2);
    rfoot << rAnklePos_[iter](0), rAnklePos_[iter](1), rAnklePos_[iter](2);
    pelvis << CoMPos_[iter](0), CoMPos_[iter](1), CoMPos_[iter](2);

    int traj_index = findTrajIndex(trajSizes_, trajSizes_.size(), iter);

    if(!bumpBiasSet_ && robotState_[iter] == 0){
        bumpBiasR_ = 0.25 * (bump_r[0] + bump_r[1] + bump_r[2] + bump_r[3]);
        bumpBiasL_ = 0.25 * (bump_l[0] + bump_l[1] + bump_l[2] + bump_l[3]);
    }
    if(iter > trajSizes_[0]){
        bumpBiasSet_ = true;
        //Foot Length Controller
        Vector3d r_wrench;
        Vector3d l_wrench;
        distributeFT(zmpd_[iter - trajSizes_[0]], rAnklePos_[iter], lAnklePos_[iter], r_wrench, l_wrench);
        cout << r_wrench(0) << ',' << r_wrench(1) << ',' << r_wrench(2) << ',' << l_wrench(0) << ',' << l_wrench(1) << ',' << l_wrench(2) << ',';
        // double delta_z = onlineWalk_.footLenController(0.0, floor((f_l - f_r) * 10) / 10, 0.0001, 1);
        double delta_z = onlineWalk_.footLenController(floor((l_wrench(0) - r_wrench(0)) * 10) / 10, floor((f_l - f_r) * 10) / 10, 0.00011, 0.0, 1.0);
        lfoot << lAnklePos_[iter](0), lAnklePos_[iter](1), lAnklePos_[iter](2) - 0.5 * delta_z;
        rfoot << rAnklePos_[iter](0), rAnklePos_[iter](1), rAnklePos_[iter](2) + 0.5 * delta_z;
        cout << zmpd_[iter - trajSizes_[0]](0) << ',' << zmpd_[iter - trajSizes_[0]](1) << ',' << zmpd_[iter - trajSizes_[0]](2) << ',';
        cout << CoMPos_[iter](0) << ',' << CoMPos_[iter](1) << ',' << CoMPos_[iter](2) << ',';
        cout << xiDesired_[iter - trajSizes_[0]](0) << ',' << xiDesired_[iter - trajSizes_[0]](1) << ',' << xiDesired_[iter- trajSizes_[0]](2) << ',';
        cout << xiDot_[iter - trajSizes_[0]](0) << ',' << xiDot_[iter - trajSizes_[0]](1) << ',' << xiDot_[iter- trajSizes_[0]](2) << ',';
        cout << delta_z << ',' << f_r << ',' << f_l << ',' << floor((f_l - f_r) * 10) / 10 << ',';
        
        //Foot Orientation Controller
        //Vector3d delta_theta = onlineWalk_.footDampingController(Vector3d::Zero(), Vector3d(0, 0, f_r), torque_r, gain, true);
        // Vector3d delta_theta_r(0, 0, 0);
        // Vector3d delta_theta_l(0, 0, 0);
        //if(robotState_[iter] == 1){
        //    delta_theta_r = onlineWalk_.footOrientController(Vector3d(r_wrench(1), r_wrench(2), 0), torque_r, 0.001, 0, 1, true);
        //    delta_theta_l = onlineWalk_.footOrientController(Vector3d(l_wrench(1), l_wrench(2), 0), torque_l, 0.001, 0, 1, false);
        //}else if(robotState_[iter] == 2){
        //    delta_theta_r = onlineWalk_.footOrientController(Vector3d(r_wrench(1), r_wrench(2), 0), torque_r, 0.001, 0, 1, true);
        //}else if(robotState_[iter] == 3){
        //    delta_theta_l = onlineWalk_.footOrientController(Vector3d(l_wrench(1), l_wrench(2), 0), torque_l, 0.001, 0, 1, false);
        //}
        //Matrix3d delta_rot_r;
        //Matrix3d delta_rot_l;
        //cout << delta_theta_r(0) << ',' << delta_theta_r(1) << ',' << delta_theta_r(2) << ',' << torque_r(0) << ',' << torque_r(1) << ','
        //     << delta_theta_l(0) << ',' << delta_theta_l(1) << ',' << delta_theta_l(2) << ',' << torque_l(0) << ',' << torque_l(1) << endl;
        //     << delta_z << ',' << f_r << ',' << f_l << ',' << floor((f_r - f_l) * 10) / 10 << endl;
        //delta_rot_r = AngleAxisd(delta_theta_r(2), Vector3d::UnitZ()) * AngleAxisd(delta_theta_r(1), Vector3d::UnitY()) * AngleAxisd(delta_theta_r(0), Vector3d::UnitX());
        //delta_rot_l = AngleAxisd(delta_theta_l(2), Vector3d::UnitZ()) * AngleAxisd(delta_theta_l(1), Vector3d::UnitY()) * AngleAxisd(delta_theta_l(0), Vector3d::UnitX());
        //rAnkleRot_[iter] = rAnkleRot_[iter] * delta_rot_r;
        //lAnkleRot_[iter] = lAnkleRot_[iter] * delta_rot_l;

        //Bump Foot Orientation Controller
        // if (robotState_[iter] == 2)
        //     delta_theta_l = onlineWalk_.bumpFootOrientController(bump_l, Vector3d::Zero(), 6.0 / 300.0, 0.0, 6.0, false);
        // else if(robotState_[iter] == 3)
        //     delta_theta_r = onlineWalk_.bumpFootOrientController(bump_r, Vector3d::Zero(), 6.0 / 300.0, 0.0, 6.0, true);

        //cout << delta_theta_r(0) << ',' << delta_theta_r(1) << ',' << delta_theta_r(2) << ',';
        //cout << delta_theta_l(0) << ',' << delta_theta_l(1) << ',' << delta_theta_l(2) << ',';
        // Matrix3d delta_rot_r;
        // Matrix3d delta_rot_l;
        // delta_rot_r = AngleAxisd(delta_theta_r(2), Vector3d::UnitZ()) * AngleAxisd(delta_theta_r(1), Vector3d::UnitY()) * AngleAxisd(delta_theta_r(0), Vector3d::UnitX());
        // delta_rot_l = AngleAxisd(delta_theta_l(2), Vector3d::UnitZ()) * AngleAxisd(delta_theta_l(1), Vector3d::UnitY()) * AngleAxisd(delta_theta_l(0), Vector3d::UnitX());
        // rAnkleRot_[iter] = rAnkleRot_[iter] * delta_rot_r;
        // lAnkleRot_[iter] = lAnkleRot_[iter] * delta_rot_l;

        //Early Contact Controller
        double r_bump_d, l_bump_d;
        distributeBump(rAnklePos_[iter](2), lAnklePos_[iter](2), r_bump_d, l_bump_d);
        Vector3d delta_r_foot = onlineWalk_.earlyContactController(bump_r, r_bump_d, 0.008, 1, true);
        Vector3d delta_l_foot = onlineWalk_.earlyContactController(bump_l, l_bump_d, 0.008, 1, false);
        //rfoot = rfoot + delta_r_foot;
        //lfoot = lfoot + delta_l_foot;
        cout << r_bump_d << ", " << l_bump_d << ", ";
        cout << bump_r[0] << ", " << bump_r[1] << ", " << bump_r[2] << ", " << bump_r[3] << ", ";
        cout << bump_l[0] << ", " << bump_l[1] << ", " << bump_l[2] << ", " << bump_l[3] << ", ";
        cout << rfoot(0) << ", " << rfoot(1) << ", " << rfoot(2) << ", "; 
        cout << lfoot(0) << ", " << lfoot(1) << ", " << lfoot(2) << ", "; 
        cout << rAnklePos_[iter](0) << ", " << rAnklePos_[iter](1) << ", " << rAnklePos_[iter](2) << ", ";
        cout << lAnklePos_[iter](0) << ", " << lAnklePos_[iter](1) << ", " << lAnklePos_[iter](2) << ", ";
        cout << delta_r_foot(0) << ", " << delta_r_foot(1) << ", " << delta_r_foot(2) << ", "; 
        cout << delta_l_foot(0) << ", " << delta_l_foot(1) << ", " << delta_l_foot(2) << endl;

        rfoot(2) = onlineWalk_.saturate<double>(COM_height_ - 0.65,COM_height_ - (thigh_ + shank_),rfoot(2));
        lfoot(2) = onlineWalk_.saturate<double>(COM_height_ - 0.65,COM_height_ - (thigh_ + shank_),lfoot(2));
    }

    if(trajContFlags_[traj_index] == true){
        Vector3d zmp_ref = onlineWalk_.dcmController(xiDesired_[iter+1], xiDot_[iter+1], realXi_[iter], COM_height_);
        Vector3d cont_out = onlineWalk_.comController(CoMPos_[iter], CoMDot_[iter+1], FKBase_[iter], zmp_ref, realZMP_[iter]);
        pelvis = cont_out;
    }

    if(ankleColide_->checkColission(lfoot, rfoot, lAnkleRot_[iter], rAnkleRot_[iter])){
        status = 1;
        cout << "Collision Detected in Ankles!" << endl;
    }
    doIK(pelvis, CoMRot_[iter], lfoot, lAnkleRot_[iter], rfoot, rAnkleRot_[iter]);

    for(int i = 0; i < 12; i++)
        joint_angles[i] = joints_[i];     // right leg(0-5) & left leg(6-11)
}

void Robot::updateState(double config[], Vector3d torque_r, Vector3d torque_l, double f_r, double f_l, Vector3d gyro, Vector3d accelerometer){
    
    // Interpret IMU data
    Vector3d change_attitude;
    //Vector3d base_attitude = links_[0]->getRot().eulerAngles(2, 1, 0); // rot(x) * rot(y) * rot(z) , eulerAngles() outputs are in the ranges [0:pi] * [-pi:pi] * [-pi:pi]
    Vector3d base_attitude = links_[0]->getEuler();
    Vector3d base_vel = links_[0]->getLinkVel();
    Vector3d base_pos = links_[0]->getPose();
    // cout << base_attitude(0) * 180/M_PI << ", " << base_attitude(1) * 180/M_PI << ", " << base_attitude(2) * 180/M_PI << endl;
    // cout << base_pos(0) << ", " << base_pos(1) << ", " << base_pos(2) << ", " << base_vel(0) << ", " << base_vel(1) << ", " << base_vel(2) << endl;

    estimator_->atitudeEulerEstimator(base_attitude, gyro);
    Matrix3d rot = (AngleAxisd(base_attitude[2], Vector3d::UnitZ()) // roll, pitch, yaw
                  * AngleAxisd(base_attitude[1], Vector3d::UnitY())
                  * AngleAxisd(base_attitude[0], Vector3d::UnitX())).matrix();
    
    //links_[0]->setEuler(base_attitude);
    links_[0]->setRot(rot);
    links_[0]->setOmega(gyro);
    /*
    Vector3d acc_g = rot * accelerometer;
    estimator_->poseVelEstimator(base_vel, base_pos, acc_g);
    links_[0]->setVel(base_vel);
    links_[0]->setPos(base_pos);
    //links_[0]->initPose(Vector3d::Zero(3), rot);
    */
    // Update swing/stance foot
    if (links_[12]->getPose()(2) < links_[6]->getPose()(2)){
        rightSwings_ = true;
        leftSwings_ = false;
    }
    else if (links_[6]->getPose()(2) < links_[12]->getPose()(2)){
        rightSwings_ = false;
        leftSwings_ = true;
    }
    else{
        rightSwings_ = false;
        leftSwings_ = false;
    }
    // Update CoM and Sole Positions
    updateSolePosition();

    // Calculate ZMP with FT data
    Vector3d l_zmp = getZMPLocal(torque_l, f_l);
    Vector3d r_zmp = getZMPLocal(torque_r, f_r);

    if (abs(f_l) < 20)
        f_l = 0;
    if (abs(f_r) < 20)
        f_r = 0;

    realZMP_[index_] = ZMPGlobal(rSole_ + l_zmp, lSole_ + r_zmp, f_r, f_l);
    //cout << realZMP_[index_](0) << "," << realZMP_[index_](1) << "," << realZMP_[index_](2) << endl;
    zmpPosition_.x = realZMP_[index_](0);
    zmpPosition_.y = realZMP_[index_](1);
    zmpPosition_.z = realZMP_[index_](2);
    zmpDataPub_.publish(zmpPosition_);
}

void Robot::updateSolePosition(){

    Matrix3d r_dot = this->rDot_(links_[0]->getRot());

    if(leftSwings_ && (!rightSwings_)){
        lSole_ = rSole_ - links_[6]->getPose() + links_[12]->getPose();
        //FKBase_[index_] = lSole_ - links_[0]->getRot() * links_[12]->getPose();
        FKBase_[index_] = lSole_ - links_[12]->getPose();
        FKBaseDot_[index_] = links_[6]->getVel().block(0, 0, 3, 1);
        //FKCoMDot_[index_] = -links_[0]->getRot() * links_[6]->getVel().block<3,1>(0, 0) - r_dot * links_[6]->getPose();
        rSoles_[index_] = rSole_;
        lSoles_[index_] = lSole_;
    }
    else if ((!leftSwings_) && rightSwings_){
        Matrix<double, 6, 1> q_dot;
        rSole_ = lSole_ - links_[12]->getPose() + links_[6]->getPose();
        //FKBase_[index_] = rSole_ - links_[0]->getRot() * links_[6]->getPose();
        FKBase_[index_] = rSole_ - links_[6]->getPose();
        FKBaseDot_[index_] = links_[12]->getVel().block(0, 0, 3, 1);
        //FKCoMDot_[index_] = -links_[0]->getRot() * links_[12]->getVel().block<3,1>(0, 0) - r_dot * links_[12]->getPose();
        rSoles_[index_] = rSole_;
        lSoles_[index_] = lSole_;
    }
    else{   // double support
        //FKBase_[index_] = rSole_ - links_[0]->getRot() * links_[6]->getPose();
        FKBase_[index_] = rSole_ - links_[6]->getPose();
        FKBaseDot_[index_] = links_[6]->getVel().block(0, 0, 3, 1);
        //FKCoMDot_[index_] = -links_[0]->getRot() * links_[6]->getVel().block<3,1>(0, 0) - r_dot * links_[6]->getPose();
        rSoles_[index_] = rSole_;
        lSoles_[index_] = lSole_;
    }
    FKCoM_[index_] = FKBase_ [index_] + CoM2Base();
    FKCoMDot_[index_] = FKBaseDot_[index_] + CoM2BaseVel();
    // 3-point backward formula for numeraical differentiation: 
    // https://www3.nd.edu/~zxu2/acms40390F15/Lec-4.1.pdf
    Vector3d f1, f0, f3, f2;
    if (index_ == 0) {
        f1 = FKCoM_[index_]; //com vel
        f0 = FKCoM_[index_]; //com vel
        f2 = Vector3d::Zero(3); //base vel
        f3 = Vector3d::Zero(3); //base vel
        
        }
    else if (index_ == 1) {
        f1 = FKCoM_[index_-1]; f0 = Vector3d::Zero(3);
        f3 = FKBase_[index_-1]; f2 = Vector3d::Zero(3);
        }
    else {
        f1 = FKCoM_[index_-1]; f0 = FKCoM_[index_-2];
        f3 = FKBase_[index_-1]; f2 = FKBase_[index_-2];
        } 
    FKBaseDot_[index_] = (f2 - 4 * f3 + 3 * FKBase_[index_])/(2 * this->dt_);
    FKCoMDot_[index_] = (f0 - 4 * f1 + 3 * FKCoM_[index_])/(2 * this->dt_);
    // if(index_ > 200){
    //     Vector3d posterior = CoMDot_[index_ - 201];
    //     double p = FKCoMDotP_[index_ - 201];
    //     estimator_->gaussianPredict(posterior, p, 0.2, CoMDot_[index_ - 200] - CoMDot_[index_ - 201]);
    //     estimator_->gaussianUpdate(posterior, p, FKCoMDot_[index_], 0.03);
    //     FKCoMDot_[index_] = posterior;
    //     FKCoMDotP_[index_ - 1] = p;
    // }
    
    //realXi_[index_] = FKBase_[index_] + FKBaseDot_[index_] / sqrt(K_G/COM_height_);
    // realXi_[index_] = FKCoM_[index_] + FKCoMDot_[index_] / sqrt(K_G/COM_height_);
    // cout << FKBase_[index_](0) << "," << FKBase_[index_](1) << "," << FKBase_[index_](2) << "," <<
    // FKCoM_[index_](0) << "," << FKCoM_[index_](1) << "," << FKCoM_[index_](2) << "," <<
    // FKBaseDot_[index_](0) << "," << FKBaseDot_[index_](1) << "," << FKBaseDot_[index_](2) << "," <<
    // FKCoMDot_[index_](0) << "," << FKCoMDot_[index_](1) << "," << FKCoMDot_[index_](2) << "," <<
    // realXi_[index_](0) << "," << realXi_[index_](1) << "," << realXi_[index_](2) << endl;
}

Vector3d Robot::getZMPLocal(Vector3d torque, double fz){
    // Calculate ZMP for each foot
    Vector3d zmp(0.0, 0.0, 0.0);
    if (fz == 0){
        //ROS_WARN("No Correct Force Value!");
        return zmp;
    }
    zmp(0) = -torque(1)/fz;
    zmp(1) = -torque(0)/fz;
    return zmp;
}

Vector3d Robot::ZMPGlobal(Vector3d zmp_r, Vector3d zmp_l, double f_r, double f_l){
    // Calculate ZMP during Double Support Phase
    Vector3d zmp(0.0, 0.0, 0.0);
    if (f_r + f_l == 0){
        //ROS_WARN("No Foot Contact, Check the Robot!");
        return zmp;
    }
    //assert(!(f_r + f_l == 0));
    return (zmp_r * f_r + zmp_l * f_l) / (f_r + f_l);
}

Vector3d Robot::CoM2Base(){
    Vector3d com;
    Vector3d mc = Vector3d::Zero();
    for(int i = 0; i < 14; i++){
        double m_i = links_[i]->getMass();
        Vector3d p_com_b = links_[i]->getPose() + links_[i]->getRot() * links_[i]->getLinkCoM();
        mc += m_i * p_com_b;
    }
    com = mc / mass_;
    return(com);
}

Vector3d Robot::CoM2BaseVel(){
    Vector3d m_p(0,0,0);
    Vector3d com_vel;
    for(int i = 0; i < 12; i ++){
        m_p += links_[i]->getMass() * (links_[0]->getOmega().cross(links_[0]->getRot() * links_[i]->getLinkCoM()) + 
                    links_[0]->getRot() * links_[i]->getLinkVel());
    }
    return m_p/ mass_;
}

void Robot::spinOffline(int iter, double* config){
    
    MatrixXd lfoot(3,1);
    MatrixXd rfoot(3,1);
    Matrix3d attitude = MatrixXd::Identity(3,3);
    MatrixXd pelvis(3,1);
    lfoot << lAnklePos_[iter](0), lAnklePos_[iter](1), lAnklePos_[iter](2);
    rfoot << rAnklePos_[iter](0), rAnklePos_[iter](1), rAnklePos_[iter](2);
    pelvis << CoMPos_[iter](0), CoMPos_[iter](1), CoMPos_[iter](2);
    doIK(pelvis,attitude,lfoot,attitude,rfoot,attitude);

    for(int i = 0; i < 12; i++)
        config[i] = joints_[i];     // right leg(0-5) & left leg(6-11)
}

void Robot::doIK(MatrixXd pelvisP, Matrix3d pelvisR, MatrixXd leftAnkleP, Matrix3d leftAnkleR, MatrixXd rightAnkleP, Matrix3d rightAnkleR){
    // Calculates and sets Robot Leg Configuration at each time step
    double* q_left = this->geometricIK(pelvisP, pelvisR, leftAnkleP, leftAnkleR, true);
    double* q_right = this->geometricIK(pelvisP, pelvisR, rightAnkleP, rightAnkleR, false);
    for(int i = 0; i < 6; i ++){
        joints_[i] = q_right[i];
        joints_[i+6] = q_left[i];
    }
    delete[] q_left;
    delete[] q_right;
}

Matrix3d Robot::Rroll(double phi){
    // helper Function for Geometric IK
    MatrixXd R(3,3);
    double c=cos(phi);
    double s=sin(phi);
    R<<1,0,0,0,c,-1*s,0,s,c;
    return R;
}

Matrix3d Robot::RPitch(double theta){
    // helper Function for Geometric IK
    MatrixXd Ry(3,3);
    double c=cos(theta);
    double s=sin(theta);
    Ry<<c,0,s,0,1,0,-1*s,0,c;
    return Ry;

}

Matrix3d Robot::rDot_(Matrix3d R){
    AngleAxisd angle_axis(R);
    Matrix3d r_dot, temp;
    temp << 0.0, -angle_axis.axis()(2), angle_axis.axis()(1),
             angle_axis.axis()(2), 0.0, -angle_axis.axis()(0),
             -angle_axis.axis()(1), angle_axis.axis()(0), 0.0;
    r_dot = temp * R;
    return r_dot;
}

double* Robot::geometricIK(MatrixXd p1, MatrixXd r1, MatrixXd p7, MatrixXd r7, bool isLeft){
    /*
        Geometric Inverse Kinematic for Robot Leg (Section 2.5  Page 53)
        Reference: Introduction to Humanoid Robotics by Kajita        https://www.springer.com/gp/book/9783642545351
        1 ----> Body        7-----> Foot
    */
   
    double* q = new double[6];  
    MatrixXd D(3,1);

    if (isLeft)
        D << 0.0,torso_,0.0;
    else
        D << 0.0,-torso_,0.0;
        
    MatrixXd r = r7.transpose() * (p1 + r1 * D - p7);
    double C = r.norm();
    double c3 = (pow(C,2) - pow(thigh_,2) - pow(shank_,2))/(2 * thigh_ * shank_);
    if (c3 >= 1){
        q[3] = 0.0;
        // Raise error
    }else if(c3 <= -1){
        q[3] = M_PI;
        // Raise error
    }else{
        q[3] = acos(c3);       // Knee Pitch
    }
    double q4a = asin((thigh_/C) * sin(M_PI - q[3]));
    q[5] = atan2(r(1,0),r(2,0));   //Ankle Roll
    if (q[5] > M_PI/2){
        q[5] = q[5] - M_PI;
        // Raise error
    }
    else if (q[5] < -M_PI / 2){
        q[5] = q[5] + M_PI;
        // Raise error
    }
    int sign_r2 = 1;
    if(r(2,0) < 0)
        sign_r2 = -1;
    q[4] = -atan2(r(0,0),sign_r2 * sqrt(pow(r(1,0),2) + pow(r(2,0),2))) - q4a;      // Ankle Pitch
    Matrix3d R = r1.transpose() * r7 * Rroll(-q[5]) * RPitch(-q[3] - q[4]);
    q[0] = atan2(-R(0,1),R(1,1));         // Hip Yaw
    q[1] = atan2(R(2,1), -R(0,1) * sin(q[0]) + R(1,1) * cos(q[0]));           // Hip Roll
    q[2] = atan2(-R(2,0), R(2,2));        // Hip Pitch
    return q;
    double* choreonoid_only = new double[6] {q[1], q[2], q[0], q[3], q[4], q[5]};
    return choreonoid_only;
}

int Robot::findTrajIndex(vector<int> arr, int n, int K)
{
    /*
        a binary search function to find the index of the running trajectory.
    */
    int start = 0;
    int end = n - 1;
    while (start <= end) {
        int mid = (start + end) / 2;
 
        if (arr[mid] == K)
            return mid + 1;
        else if (arr[mid] < K)
            start = mid + 1;
        else
            end = mid - 1;
    }
    return end + 1;
}

void Robot::distributeFT(Vector3d zmp, Vector3d r_foot,Vector3d l_foot, Vector3d &r_wrench, Vector3d &l_wrench){
    
    double k_f = abs((zmp(1) - r_foot(1))) / abs((r_foot(1) - l_foot(1)));
    l_wrench(0) = k_f * mass_ * K_G;
    r_wrench(0) = (1 - k_f) * mass_ * K_G;

    l_wrench(1) = l_wrench(0) * (zmp(1) - l_foot(1));
    r_wrench(1) = r_wrench(0) * (zmp(1) - r_foot(1));

    l_wrench(2) = l_wrench(0) * (zmp(0) - l_foot(0));
    r_wrench(2) = r_wrench(0) * (zmp(0) - r_foot(0));
}

void Robot::distributeBump(double r_foot_z, double l_foot_z, double &r_bump, double &l_bump){
    r_bump = max(bumpBiasR_, min(0.0, (bumpBiasR_ / 0.02) * (0.02 - r_foot_z)));
    l_bump = max(bumpBiasL_, min(0.0, (bumpBiasL_ / 0.02) * (0.02 - l_foot_z)));
}

bool Robot::trajGenCallback(trajectory_planner::Trajectory::Request  &req,
                            trajectory_planner::Trajectory::Response &res)
{
    /*
        ROS service for generating robot COM & ankles trajectories
    */
    //ROS_INFO("Generating Trajectory started.");
    //auto start = high_resolution_clock::now();
    int trajectory_size = int(((req.step_count + 2) * req.t_step) / req.dt);
    double alpha = req.alpha;
    double t_ds = req.t_double_support;
    double t_s = req.t_step;
    COM_height_ = req.COM_height;
    double step_len = req.step_length;
    double step_width = req.step_width;
    int num_step = req.step_count;
    dt_ = req.dt;
    float theta = req.theta;
    double swing_height = req.ankle_height;
    double init_COM_height = thigh_ + shank_;  // SURENA IV initial height 
    
    DCMPlanner* trajectoryPlanner = new DCMPlanner(COM_height_, t_s, t_ds, dt_, num_step + 2, alpha, theta);
    Ankle* anklePlanner = new Ankle(t_s, t_ds, swing_height, alpha, num_step, dt_, theta);
    Vector3d* dcm_rf = new Vector3d[num_step + 2];  // DCM rF
    Vector3d* ankle_rf = new Vector3d[num_step + 2]; // Ankle rF
    int sign;
    if(theta == 0.0){   // Straight or Diagonal Walk
        int sign;
        if (step_width == 0){sign = 1;}
        else{sign = (step_width/abs(step_width));}

        ankle_rf[0] << 0.0, (torso_ + 0.0) * sign, 0.0;
        ankle_rf[1] << 0.0, (torso_ + 0.0) * -sign, 0.0;
        dcm_rf[0] << 0.0, 0.0, 0.0;
        dcm_rf[1] << 0.0, torso_ * -sign, 0.0;
        //cout << dcm_rf[0](0) << ", " << dcm_rf[0](1) << ", " << dcm_rf[0](2) << ", " << ankle_rf[0](0) << ", " << ankle_rf[0](1) << ", " << ankle_rf[0](2) << endl;
        //cout << dcm_rf[1](0) << ", " << dcm_rf[1](1) << ", " << dcm_rf[1](2) << ", " << ankle_rf[1](0) << ", " << ankle_rf[1](1) << ", " << ankle_rf[1](2) << endl;
        for(int i = 2; i <= num_step + 1; i ++){
            if (i == 2 || i == num_step + 1){
                ankle_rf[i] = ankle_rf[i-2] + Vector3d(step_len, step_width, 0.0);
                dcm_rf[i] << ankle_rf[i-2] + Vector3d(step_len, step_width, 0.0);
            }else{
                ankle_rf[i] = ankle_rf[i-2] + Vector3d(2 * step_len, step_width, 0.0);
                dcm_rf[i] << ankle_rf[i-2] + Vector3d(2 * step_len, step_width, 0.0);
            }
            //cout << dcm_rf[i](0) << ", " << dcm_rf[i](1) << ", " << dcm_rf[i](2) << ", " << ankle_rf[i](0) << ", " << ankle_rf[i](1) << ", " << ankle_rf[i](2) << endl;
        }
        dcm_rf[num_step + 1] = 0.5 * (ankle_rf[num_step] + ankle_rf[num_step + 1]);
        ankle_rf[0] << 0.0, torso_ * sign, 0.0;
        ankle_rf[1] << 0.0, torso_ * -sign, 0.0;
    }  

    else{   //Turning Walk
        double r = abs(step_len/theta);
        sign = abs(step_len) / step_len;
        //cout << sign << endl;
        ankle_rf[0] = Vector3d(0.0, -sign * torso_, 0.0);
        dcm_rf[0] = Vector3d::Zero(3);
        ankle_rf[num_step + 1] = (r + pow(-1, num_step) * torso_) * Vector3d(sin(theta * (num_step-1)), sign * cos(theta * (num_step-1)), 0.0) + 
                                    Vector3d(0.0, -sign * r, 0.0);
        //cout << ankle_rf[0](0) << "," << ankle_rf[0](1) << "," << ankle_rf[0](2) << endl;
        for (int i = 1; i <= num_step; i ++){
            ankle_rf[i] = (r + pow(-1, i-1) * torso_) * Vector3d(sin(theta * (i-1)), sign * cos(theta * (i-1)), 0.0) + 
                                    Vector3d(0.0, -sign * r, 0.0);
            dcm_rf[i] = ankle_rf[i];
            //cout << dcm_rf[i](0) << "," << dcm_rf[i](1) << "," << dcm_rf[i](2) << endl ;
        }
        dcm_rf[num_step + 1] = 0.5 * (ankle_rf[num_step] + ankle_rf[num_step + 1]);
        //cout << ankle_rf[num_step + 1](0) << "," << ankle_rf[num_step + 1](1) << "," << ankle_rf[num_step + 1](2) << endl ;

        //if (theta > 0){
        //    dcm_rf[1] << 0.0, -torso_, 0.0;
        //    ankle_rf[1] << 0.0, -torso_, 0.0;
        //}
        //else{
        //    dcm_rf[1] << 0.0, torso_, 0.0;
        //    ankle_rf[1] << 0.0, torso_, 0.0;  
        //}
        //for (int i = 1; i < num_step; i++){
        //    if (theta >= 0){
        //        dcm_rf[i+1] << dcm_rf[i](0) + cos(i * theta) * step_len - sin(i * theta) * pow(-1, i + 1) * torso_ , sin(i * theta) * (step_len) + cos(i * theta) * pow(-1, i + 1) * torso_, 0.0;
        //        ankle_rf[i+1] << dcm_rf[i](0) + cos(i * theta) * step_len - sin(i * theta) * pow(-1, i + 1) * torso_ , sin(i * theta) * (step_len) + cos(i * theta) * pow(-1, i + 1) * torso_, 0.0;
        //    }
        //    else {
        //        dcm_rf[i+1] << dcm_rf[i](0) + cos(i * theta) * step_len + sin(i * theta) * pow(-1, i + 1) * torso_ , sin(i * theta) * (step_len) - cos(i * theta) * pow(-1, i + 1) * torso_, 0.0;
        //        ankle_rf[i+1] << dcm_rf[i](0) + cos(i * theta) * step_len + sin(i * theta) * pow(-1, i + 1) * torso_ , sin(i * theta) * (step_len) - cos(i * theta) * pow(-1, i + 1) * torso_, 0.0;
        //    }
        //}

        //double final_theta = (num_step - 1) * theta;
        //dcm_rf[0] << 0.0, 0.0, 0.0;
        //ankle_rf[0] << 0.0, -ankle_rf[1](1), 0.0;
        //if (theta >= 0)
        //    ankle_rf[num_step + 1] << ankle_rf[num_step](0) + pow(-1, num_step) * 2 * torso_ * sin(final_theta), ankle_rf[num_step](1) - pow(-1, num_step) * 2 * torso_ * cos(final_theta), 0.0;
        //else
        //    ankle_rf[num_step + 1] << ankle_rf[num_step](0) - pow(-1, num_step) * 2 * torso_ * sin(final_theta), ankle_rf[num_step](1) + pow(-1, num_step) * 2 * torso_ * cos(final_theta), 0.0;
        //dcm_rf[num_step + 1] << (ankle_rf[num_step + 1](0) + ankle_rf[num_step](0)) / 2, (ankle_rf[num_step + 1](1) + ankle_rf[num_step](1)) / 2, 0.0;
    }
    trajectoryPlanner->setFoot(dcm_rf, -sign);
    xiDesired_ = trajectoryPlanner->getXiTrajectory();
    zmpd_ = trajectoryPlanner->getZMP();
    xiDot_ = trajectoryPlanner->getXiDot();
    delete[] dcm_rf;
    anklePlanner->updateFoot(ankle_rf, -sign);
    anklePlanner->generateTrajectory();
    delete[] ankle_rf;
    onlineWalk_.setDt(req.dt);
    estimator_->setDt(req.dt);
    onlineWalk_.setBaseHeight(req.COM_height);
    onlineWalk_.setBaseIdle(shank_ + thigh_);
    onlineWalk_.setBaseLowHeight(0.65);
    onlineWalk_.setInitCoM(Vector3d(0.0,0.0,COM_height_));

    if (dataSize_ != 0){
        dataSize_ += trajectory_size;

        CoMPos_ = appendTrajectory<Vector3d>(CoMPos_, trajectoryPlanner->getCoM(), dataSize_ - trajectory_size, trajectory_size);
        lAnklePos_ = appendTrajectory<Vector3d>(lAnklePos_, anklePlanner->getTrajectoryL(), dataSize_ - trajectory_size, trajectory_size);
        rAnklePos_ = appendTrajectory<Vector3d>(rAnklePos_, anklePlanner->getTrajectoryR(), dataSize_ - trajectory_size, trajectory_size);
        robotState_ = appendTrajectory<int>(robotState_, anklePlanner->getRobotState(), dataSize_ - trajectory_size, trajectory_size);

        CoMRot_ = appendTrajectory<Matrix3d>(CoMRot_, trajectoryPlanner->yawRotGen(), dataSize_ - trajectory_size, trajectory_size);
        lAnkleRot_ = appendTrajectory<Matrix3d>(lAnkleRot_, anklePlanner->getRotTrajectoryL(), dataSize_ - trajectory_size, trajectory_size);
        rAnkleRot_ = appendTrajectory<Matrix3d>(rAnkleRot_, anklePlanner->getRotTrajectoryR(), dataSize_ - trajectory_size, trajectory_size);      

        delete[] FKBase_;
        delete[] FKCoM_;
        delete[] FKCoMDot_;
        delete[] FKCoMDotP_;
        delete[] FKBaseDot_;
        delete[] realXi_;
        delete[] realZMP_;
        delete[] rSoles_;
        delete[] lSoles_;
        
    }else{
        dataSize_ += trajectory_size;

        CoMPos_ = trajectoryPlanner->getCoM();
        lAnklePos_ = anklePlanner->getTrajectoryL();
        rAnklePos_ = anklePlanner->getTrajectoryR();
        robotState_ = anklePlanner->getRobotState();

        CoMRot_ = trajectoryPlanner->yawRotGen();
        lAnkleRot_ = anklePlanner->getRotTrajectoryR();
        rAnkleRot_ = anklePlanner->getRotTrajectoryL();
    }
    CoMDot_ = trajectoryPlanner->get_CoMDot();
    //ROS_INFO("trajectory generated");
    res.result = true;
    trajSizes_.push_back(dataSize_);
    trajContFlags_.push_back(false);
    isTrajAvailable_ = true;

    FKBase_ = new Vector3d[dataSize_];
    FKCoM_ = new Vector3d[dataSize_];
    FKCoMDot_ = new Vector3d[dataSize_];
    FKCoMDotP_ = new double[dataSize_];
    FKCoMDotP_[0] = 2.0;
    FKBaseDot_ = new Vector3d[dataSize_];
    realXi_ = new Vector3d[dataSize_];
    realZMP_ = new Vector3d[dataSize_];
    rSoles_ = new Vector3d[dataSize_];
    lSoles_ = new Vector3d[dataSize_];
    //auto stop = high_resolution_clock::now();
    //auto duration = duration_cast<microseconds>(stop - start);
    //cout << duration.count()/1000000.0 << endl;
    return true;
}

bool Robot::generalTrajCallback(trajectory_planner::GeneralTraj::Request  &req,
                                trajectory_planner::GeneralTraj::Response &res)
{
    dt_ = req.dt;
    GeneralMotion* motion_planner = new GeneralMotion(dt_);
    motion_planner->changeInPlace(Vector3d(req.init_com_pos[0], req.init_com_pos[1], req.init_com_pos[2]), 
                                  Vector3d(req.final_com_pos[0], req.final_com_pos[1], req.final_com_pos[2]), 
                                  Vector3d(req.init_com_orient[0], req.init_com_orient[1], req.init_com_orient[2]), 
                                  Vector3d(req.final_com_orient[0], req.final_com_orient[1], req.final_com_orient[2]),
                                  Vector3d(req.init_lankle_pos[0], req.init_lankle_pos[1], req.init_lankle_pos[2]), 
                                  Vector3d(req.final_lankle_pos[0], req.final_lankle_pos[1], req.final_lankle_pos[2]),
                                  Vector3d(req.init_lankle_orient[0], req.init_lankle_orient[1], req.init_lankle_orient[2]), 
                                  Vector3d(req.final_lankle_orient[0], req.final_lankle_orient[1], req.final_lankle_orient[2]),
                                  Vector3d(req.init_rankle_pos[0], req.init_rankle_pos[1], req.init_rankle_pos[2]), 
                                  Vector3d(req.final_rankle_pos[0], req.final_rankle_pos[1], req.final_rankle_pos[2]),
                                  Vector3d(req.init_rankle_orient[0], req.init_rankle_orient[1], req.init_rankle_orient[2]), 
                                  Vector3d(req.final_rankle_orient[0], req.final_rankle_orient[1], req.final_rankle_orient[2]),
                                  req.time);
    int trajectory_size = motion_planner->getLength();

    onlineWalk_.setDt(req.dt);
    estimator_->setDt(req.dt);
    onlineWalk_.setBaseIdle(shank_ + thigh_);
    onlineWalk_.setBaseLowHeight(0.65);
    onlineWalk_.setInitCoM(Vector3d(0.0,0.0,COM_height_));

    if (dataSize_ != 0){
        dataSize_ += trajectory_size;

        CoMPos_ = appendTrajectory<Vector3d>(CoMPos_, motion_planner->getCOMPos(), dataSize_ - trajectory_size, trajectory_size);
        lAnklePos_ = appendTrajectory<Vector3d>(lAnklePos_, motion_planner->getLAnklePos(), dataSize_ - trajectory_size, trajectory_size);
        rAnklePos_ = appendTrajectory<Vector3d>(rAnklePos_, motion_planner->getRAnklePos(), dataSize_ - trajectory_size, trajectory_size);
        robotState_ = appendTrajectory<int>(robotState_, motion_planner->getRobotState(), dataSize_ - trajectory_size, trajectory_size);

        CoMRot_ = appendTrajectory<Matrix3d>(CoMRot_, motion_planner->getCOMOrient(), dataSize_ - trajectory_size, trajectory_size);
        lAnkleRot_ = appendTrajectory<Matrix3d>(lAnkleRot_, motion_planner->getLAnkleOrient(), dataSize_ - trajectory_size, trajectory_size);
        rAnkleRot_ = appendTrajectory<Matrix3d>(rAnkleRot_, motion_planner->getRAnkleOrient(), dataSize_ - trajectory_size, trajectory_size);      

        delete[] FKBase_;
        delete[] FKCoM_;
        delete[] FKCoMDot_;
        delete[] FKCoMDotP_;
        delete[] FKBaseDot_;
        delete[] realXi_;
        delete[] realZMP_;
        delete[] rSoles_;
        delete[] lSoles_;
        
    }else{
        dataSize_ += trajectory_size;

        CoMPos_ = motion_planner->getCOMPos();
        lAnklePos_ = motion_planner->getLAnklePos();
        rAnklePos_ = motion_planner->getRAnklePos();
        robotState_ = motion_planner->getRobotState();

        CoMRot_ = motion_planner->getCOMOrient();
        lAnkleRot_ = motion_planner->getLAnkleOrient();
        rAnkleRot_ = motion_planner->getRAnkleOrient();
    }

    res.duration = dataSize_;
    trajSizes_.push_back(dataSize_);
    trajContFlags_.push_back(false);
    isTrajAvailable_ = true;
    FKBase_ = new Vector3d[dataSize_];
    FKCoM_ = new Vector3d[dataSize_];
    FKCoMDot_ = new Vector3d[dataSize_];
    FKCoMDotP_ = new double[dataSize_];
    FKCoMDotP_[0] = 2.0;
    FKBaseDot_ = new Vector3d[dataSize_];
    realXi_ = new Vector3d[dataSize_];
    realZMP_ = new Vector3d[dataSize_];
    rSoles_ = new Vector3d[dataSize_];
    lSoles_ = new Vector3d[dataSize_];
    return true;
}

bool Robot::jntAngsCallback(trajectory_planner::JntAngs::Request  &req,
                            trajectory_planner::JntAngs::Response &res)
{
    /*
        ROS service for returning joint angles. before calling this service, 
        you must first call traj_gen service. 
    */
    if (isTrajAvailable_)
    {
        index_ = req.iter;
        double jnt_angs[12];
        Vector3d right_torque(req.right_ft[1], req.right_ft[2], 0.0);
        Vector3d left_torque(req.left_ft[1], req.left_ft[2], 0.0);
        double config[13];
        double jnt_vel[13];
        int right_bump[4];
        int left_bump[4];
        config[0] = 0;     //Pelvis joint angle
        jnt_vel[0] = 0;  //Pelvis joint velocity
        for(int i = 1; i < 13; i++){
            config[i] = req.config[i-1];
            jnt_vel[i] = req.jnt_vel[i-1];  
            if(i < 5){
                right_bump[i-1] = req.right_bump[i-1];
                left_bump[i-1] = req.left_bump[i-1];
            }
        }
        //cout << req.right_ft[0] << "," << req.right_ft[1] << ","  << req.right_ft[2] << "," <<  
        //req.left_ft[0] << "," << req.left_ft[1] << ","  << req.left_ft[2] << "," << 
        //cout << req.config[0] << "," << req.config[1] << "," << req.config[2] << ","  << req.config[3] << "," << 
        //req.config[4] << "," << req.config[5] << ","  << req.config[6] << "," << 
        //req.config[7] << "," << req.config[8] << ","  << req.config[9] << "," <<
        //req.config[10] << "," << req.config[11] << "," << endl;
        //cout << req.accelerometer[0] << "," << req.accelerometer[1] << ","  << req.accelerometer[2] << "," <<
        //req.gyro[0] << "," << req.gyro[1] << ","  << req.gyro[2] << ","<< req.left_ft[0]<< ",";
        //cout << req.right_ft[0] << "," << req.right_ft[1] << "," << req.right_ft[2] << ","
        //<< req.left_ft[0] << "," << req.left_ft[1] << "," << req.left_ft[2] << "," << endl;
        int status = 0;     // 0: Okay, 1: Ankle Collision
        this->spinOnline(req.iter, config, jnt_vel, right_torque, left_torque, req.right_ft[0], req.left_ft[0],
                         Vector3d(req.gyro[0], req.gyro[1], req.gyro[2]),
                         Vector3d(req.accelerometer[0],req.accelerometer[1],req.accelerometer[2]),
                         right_bump, left_bump, jnt_angs, status);
        res.status = status;
        //this->spinOffline(req.iter, jnt_angs);
        for(int i = 0; i < 12; i++)
            res.jnt_angs[i] = jnt_angs[i];
        //ROS_INFO("joint angles requested");
    }else{
        ROS_INFO("First call traj_gen service");
        return false;
    }
    
    if (req.iter == dataSize_ - 1 ){ 
        write2File(FKBase_, dataSize_,"CoM Real");
        write2File(realZMP_, dataSize_, "ZMP Real");
        write2File(realXi_, dataSize_, "Xi Real");
        write2File(FKCoMDot_, dataSize_, "CoM Velocity Real");
        write2File(rSoles_, dataSize_, "Right Sole");
        write2File(lSoles_, dataSize_, "Left Sole");
    }
    //ROS_INFO("joint angles returned");
    return true;
}

bool Robot::resetTrajCallback(std_srvs::Empty::Request  &req,
                              std_srvs::Empty::Response &res)
{
    delete[] CoMPos_;
    delete[] robotState_;
    delete[] lAnklePos_;
    delete[] rAnklePos_;
    delete[] CoMRot_;
    delete[] lAnkleRot_;
    delete[] rAnkleRot_;

    delete[] FKBase_;
    delete[] FKCoM_;
    delete[] FKCoMDot_;
    delete[] FKCoMDotP_;
    delete[] FKBaseDot_;
    delete[] realXi_;
    delete[] realZMP_;
    delete[] rSoles_;
    delete[] lSoles_;

    trajSizes_.clear();
    trajContFlags_.clear();
    dataSize_ = 0;
    rSole_ << 0.0, -torso_, 0.0;
    lSole_ << 0.0, torso_, 0.0; 
    isTrajAvailable_ = false;
    Vector3d position(0.0, 0.0, 0.0);
    links_[0]->initPose(position, Matrix3d::Identity(3, 3));
    return true;
}

Robot::~Robot(){
    delete ankleColide_;
    //delete[] links_;
    //delete[] FKBase_;
}

int main(int argc, char* argv[])
{
    ros::init(argc, argv, "trajectory_node");
    ros::NodeHandle nh;
    Matrix3d kp, ki, kcom, kzmp;
    kp << 1,0,0,0,1,0,0,0,0;
    ki = MatrixXd::Zero(3, 3);
    kcom = MatrixXd::Zero(3, 3);
    kzmp = MatrixXd::Zero(3, 3);
    //kcom << 4,0,0,0,4,0,0,0,0;
    //kzmp << 0.5,0,0,0,0.5,0,0,0,0;
    Controller default_ctrl(kp, ki, kzmp, kcom);
    Robot surena(&nh, default_ctrl);
    ros::spin();
}