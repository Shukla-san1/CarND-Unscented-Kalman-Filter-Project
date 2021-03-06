#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

#define SMALL_FLOAT_VAL 0.0001
using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;


/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 0.5;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 0.5;

  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;

 //variable initlizations
  is_initialized_ = false;
  time_us_ = 0;
  n_x_ = 5;
  n_aug_ = 7;
  lambda_ = 3 - n_aug_;
  n_z_ =3;
  NIS_lidar = 0.0;
  NIS_radar = 0.0;
  weights_ = VectorXd(2*n_aug_+1);
  weights_.fill(0.0);
  x_.fill(1.0);
  P_ << 1,0,0,0,0,
		0,1,0,0,0,
		0,0,1,0,0,
		0,0,0,1,0,
		0,0,0,0,1;
  Xsig_pred_ = MatrixXd(n_x_, 2 * n_aug_ + 1);
  Xsig_pred_.fill(0.0);
  Zsig_ = MatrixXd(n_z_, 2 * n_aug_ + 1);
  Zsig_.fill(0.0);
  // initializing matrices
  R_laser_ = MatrixXd(2, 2);
  H_laser_ = MatrixXd(2, 5);

  H_laser_ << 1, 0, 0, 0,0,
    		0, 1, 0, 0,0;
  R_laser_ << std_laspx_ * std_laspx_ , 0,
		  	  0,std_laspy_*std_laspy_;
}

UKF::~UKF() {}


void UKF::ProcessMeasurement(MeasurementPackage meas_package) {


	/*****************************************************************************
	   *  Initialization
	   ****************************************************************************/
	  if (!is_initialized_) {


		//define variable to receive RADAR and LIDAR position data

		float px = 0.0;
		float py = 0.0;
		float vx = 0.0;
		float vy = 0.0;



	    //initialize weight
	    SetWeight();

	    if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
	      /**
	      Convert radar from polar to cartesian coordinates and initialize state.
	      */


				float ro = meas_package.raw_measurements_[0];
				float phi = meas_package.raw_measurements_[1];
				float ro_dot = meas_package.raw_measurements_[2];

				//Normalize Phi
				phi = NormalizePhi(phi);

				px = ro * cos(phi);
				py = ro * sin(phi);
				vx = ro_dot * cos(phi);
				vy = ro_dot * sin(phi);

				x_ << px, py, sqrt(vx*vx + vy*vy),phi,0;

	    }

	    else if (meas_package.sensor_type_ == MeasurementPackage::LASER) {
	      /**
	      Initialize state.
	      */
			 px = meas_package.raw_measurements_[0];
			 py = meas_package.raw_measurements_[1];
			 x_ << px, py, 0,0,0;
	    }



		//Check for zeros
		  if (fabs(x_(0)) < SMALL_FLOAT_VAL and fabs(x_(1)) < SMALL_FLOAT_VAL){
			  x_(0) = 0.0001;
			  x_(1) = 0.0001;
		  }

	    //add new time stamp from measurment_pack
		  time_us_ = meas_package.timestamp_;


	    // done initializing, no need to predict or update
	    is_initialized_ = true;
	    return;
	  }

	  /*****************************************************************************
	  	 *  Calculate dt
	  	 ****************************************************************************/

	  	float dt = (meas_package.timestamp_ - time_us_) / 1000000.0;
	  	time_us_ = meas_package.timestamp_;


	  /*****************************************************************************
	   *  Prediction
	   ****************************************************************************/

	  	while (dt > 0.2)
	  		{
	  		double step = 0.1;
	  	    Prediction(step);
	  	    dt -= step;
	  		}

	  	Prediction(dt);

	/*****************************************************************************
	   *  Update
	   ****************************************************************************/

	  /**
		 * Use the sensor type to perform the update step.
		 * Update the state and covariance matrices.
	   */

	  if (meas_package.sensor_type_ == MeasurementPackage::RADAR)
		{
		    UpdateRadar(meas_package);

		}
		else
		{
			UpdateLidar(meas_package);
	    }

	  // print the output

	  cout << "x_ = " << x_ << endl;
	  cout << "P_ = " << P_ << endl;
}


void UKF::Prediction(double delta_t) {

 /*
  * In this function we first create augmented sigma points and then predict sigma points , Using predicted sigmapoints then predict state and covariance.
  * Input: delta_t between to sensor reading
  * Output:
  * x_: state vector
  * P_: Covariance matrix.
  */

	MatrixXd Xsig_aug = MatrixXd(n_aug_, 2 * n_aug_ + 1);
	Xsig_aug.fill(0.0);
	AugmentedSigmaPoints(&Xsig_aug);
	Xsig_pred_ = SigmaPointPrediction(Xsig_aug, delta_t);
	PredictMeanAndCovariance(&x_ , &P_);

}


void UKF::UpdateLidar(MeasurementPackage meas_package) {
	/*
	  * This function is same as EKF update function for Lidar and used to update the state and covariance.
	  * Input: MeasurementPackage object
	  * Output:
	  * x_: state vector
	  * P_: Covariance matrix.
	  * NIS_lidar
	  */



	float px = meas_package.raw_measurements_[0];
	float py = meas_package.raw_measurements_[1];
	VectorXd z = VectorXd(2);
    z << px,py;

	VectorXd z_pred = H_laser_ * x_;
	VectorXd y = z - z_pred;
	MatrixXd Ht = H_laser_.transpose();
	MatrixXd PHt = P_ * Ht;
	MatrixXd S = H_laser_ * PHt + R_laser_;
	MatrixXd Si = S.inverse();
	MatrixXd K = PHt * Si;

	//new estimate
	x_ = x_ + (K * y);
	long x_size = x_.size();
	MatrixXd I = MatrixXd::Identity(x_size, x_size);
	P_ = (I - K * H_laser_) * P_;

	// NIS calculation for Lidar

	NIS_lidar = y.transpose()*Si*y;
	std::cout << "NIS_lidar : " << NIS_lidar << std::endl;
}

void UKF::UpdateRadar(MeasurementPackage meas_package) {
	/*
	  * This function is  used to update the state and covariance for Radar.
	  * Input: MeasurementPackage object
	  * Output:
	  * x_: state vector
	  * P_: Covariance matrix.
	  * NIS_lidar
	  */

	float ro = meas_package.raw_measurements_[0];
	float phi = meas_package.raw_measurements_[1];
	phi = NormalizePhi(phi);
	float ro_dot = meas_package.raw_measurements_[2];

	VectorXd z = VectorXd(n_z_);

	z << ro,phi,ro_dot;

	 //mean predicted measurement
	  VectorXd z_pred = VectorXd(n_z_);
	  MatrixXd S = MatrixXd(n_z_,n_z_);

	  PredictRadarMeasurement(&z_pred , &S);

	  //create matrix for cross correlation Tc
	  MatrixXd Tc = MatrixXd(n_x_, n_z_);


	  //calculate cross correlation matrix
	  Tc.fill(0.0);
	  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points


		VectorXd z_diff = Zsig_.col(i) - z_pred;
		//angle normalization
		z_diff(1) = NormalizePhi(z_diff(1));


		// state difference
		VectorXd x_diff = Xsig_pred_.col(i) - x_;

		//angle normalization
		x_diff(3) = NormalizePhi(x_diff(3));

		Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
	  }

	  //Kalman gain K;
	  MatrixXd K = Tc * S.inverse();

	  //residual
	  VectorXd z_diff = z - z_pred;

	  //angle normalization
	  z_diff(1) = NormalizePhi(z_diff(1));


	  //update state mean and covariance matrix
	  x_ = x_ + K * z_diff;
	  P_ = P_ - K*S*K.transpose();



	  //print result
	  std::cout << "Updated state x: " << std::endl << x_ << std::endl;
	  std::cout << "Updated state covariance P: " << std::endl << P_ << std::endl;

	  //NIS calculation for Radar
	  NIS_radar = z_diff.transpose()*S.inverse()*z_diff;
	  std::cout << "NIS_radar : "<< NIS_radar << std::endl;
}



void UKF::AugmentedSigmaPoints(MatrixXd* Xsig_out) {
	/*
		  * This function is  used to create augmented sigma points
		  * Input: Pointer to matrix to store the aumented points
		  * Output: Void
		  *
	*/


  //create augmented mean vector
  VectorXd x_aug = VectorXd(7);

  //create augmented state covariance
  MatrixXd P_aug = MatrixXd(7, 7);

  //create sigma point matrix
  MatrixXd Xsig_aug = MatrixXd(n_aug_, 2 * n_aug_ + 1);


  //create augmented mean state
  x_aug.head(5) = x_;
  x_aug(5) = 0;
  x_aug(6) = 0;

  //create augmented covariance matrix
  P_aug.fill(0.0);
  P_aug.topLeftCorner(5,5) = P_;
  P_aug(5,5) = std_a_*std_a_;
  P_aug(6,6) = std_yawdd_*std_yawdd_;

  //create square root matrix
  MatrixXd L = P_aug.llt().matrixL();

  //create augmented sigma points
  Xsig_aug.col(0)  = x_aug;
  for (int i = 0; i< n_aug_; i++)
  {
    Xsig_aug.col(i+1)       = x_aug + sqrt(lambda_+n_aug_) * L.col(i);
    Xsig_aug.col(i+1+n_aug_) = x_aug - sqrt(lambda_+n_aug_) * L.col(i);
  }

  //print result
  std::cout << "Xsig_aug = " << std::endl << Xsig_aug << std::endl;

  //write result
  *Xsig_out = Xsig_aug;



}

MatrixXd UKF::SigmaPointPrediction(MatrixXd Xsig_aug, double delta_t) {

	/*
		  * This function is  used for prediction of sigma points.
		  * Input:
		     * Xsig_aug: Matrix of augmented sigma points
		     * Delat_t: times difference between two measurments.
		  * Output:
		     * Matrix of size 5X15

 */


  MatrixXd Xsig_pred = MatrixXd(n_x_, 2 * n_aug_ + 1);

  //predict sigma points
  for (int i = 0; i< 2*n_aug_+1; i++)
  {
    //extract values for better readability
    double p_x = Xsig_aug(0,i);
    double p_y = Xsig_aug(1,i);
    double v = Xsig_aug(2,i);
    double yaw = Xsig_aug(3,i);
    double yawd = Xsig_aug(4,i);
    double nu_a = Xsig_aug(5,i);
    double nu_yawdd = Xsig_aug(6,i);

    //predicted state values
    double px_p, py_p;

    //avoid division by zero
    if (fabs(yawd) > 0.001) {
        px_p = p_x + v/yawd * ( sin (yaw + yawd*delta_t) - sin(yaw));
        py_p = p_y + v/yawd * ( cos(yaw) - cos(yaw+yawd*delta_t) );
    }
    else {
        px_p = p_x + v*delta_t*cos(yaw);
        py_p = p_y + v*delta_t*sin(yaw);
    }

    double v_p = v;
    double yaw_p = yaw + yawd*delta_t;
    double yawd_p = yawd;

    //add noise
    px_p = px_p + 0.5*nu_a*delta_t*delta_t * cos(yaw);
    py_p = py_p + 0.5*nu_a*delta_t*delta_t * sin(yaw);
    v_p = v_p + nu_a*delta_t;

    yaw_p = yaw_p + 0.5*nu_yawdd*delta_t*delta_t;
    yawd_p = yawd_p + nu_yawdd*delta_t;

    //write predicted sigma point into right column
    Xsig_pred(0,i) = px_p;
    Xsig_pred(1,i) = py_p;
    Xsig_pred(2,i) = v_p;
    Xsig_pred(3,i) = yaw_p;
    Xsig_pred(4,i) = yawd_p;
  }


  //print result
  std::cout << "Xsig_pred = " << std::endl << Xsig_pred << std::endl;

  return Xsig_pred;

}

void UKF::SetWeight()
{
	/*
		  * This function is  used to set weights vector which can be calculated once and used later in diffferent other functions.

	*/

	 double weight_0 = lambda_/(lambda_+n_aug_);
	  weights_(0) = weight_0;
	  for (int i=1; i<2*n_aug_+1; i++) {
	    double weight = 0.5/(n_aug_+lambda_);
	    weights_(i) = weight;
	  }
}

void UKF::PredictMeanAndCovariance(VectorXd* x_out, MatrixXd* P_out) {

	/*
		  * This function is  used to predict mean and covariance.
		  * Input:
		        *  Pointer to matrix to store the covariance matrix.
		        *  Pointer to Vector to store mean
		   * Output:
		      * update predicted state mean
		      * update Corvariance matrix of size 5X5

   */



  //create vector for predicted state
  VectorXd x = VectorXd(n_x_);

  //create covariance matrix for prediction
  MatrixXd P = MatrixXd(n_x_, n_x_);



  //predicted state mean
  x.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //iterate over sigma points
    x = x+ weights_(i) * Xsig_pred_.col(i);
  }

  //predicted state covariance matrix
  P.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //iterate over sigma points

    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x;

    //angle normalization
    x_diff(3)= NormalizePhi(x_diff(3));


    P = P + weights_(i) * x_diff * x_diff.transpose() ;
  }



  //write result
  *x_out = x;
  *P_out = P;
}

void UKF::PredictRadarMeasurement(VectorXd* z_out, MatrixXd* S_out) {

	/*
		  * This function is  used to transform sigma points into meausurements space and calculate predicted mean and measurments covariances matrix.
		  * Input:
		         * Pointer to Matrix to store measurements covariance matrix.
		         * Pointer to Vector to store mean predicted measurements.
		   * Output:
		        *  calculate and update the measurements covariance matrix
		        *  Calculate and update store mean predicted measurements.

	 */



  //transform sigma points into measurement space
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

    // extract values for better readibility
    double p_x = Xsig_pred_(0,i);
    double p_y = Xsig_pred_(1,i);
    double v  = Xsig_pred_(2,i);
    double yaw = Xsig_pred_(3,i);

    double v1 = cos(yaw)*v;
    double v2 = sin(yaw)*v;
    //check division by zero
    if (fabs(p_x) < SMALL_FLOAT_VAL and fabs(p_y) < SMALL_FLOAT_VAL){
    	p_x = 0.0001;
    	p_y = 0.0001;
	  }

    // measurement model
    Zsig_(0,i) = sqrt(p_x*p_x + p_y*p_y);                        //r
    Zsig_(1,i) = atan2(p_y,p_x);                                 //phi
    Zsig_(2,i) = (p_x*v1 + p_y*v2 ) / sqrt(p_x*p_x + p_y*p_y);   //r_dot
  }

  //mean predicted measurement
  VectorXd z_pred = VectorXd(n_z_);
  z_pred.fill(0.0);
  for (int i=0; i < 2*n_aug_+1; i++) {
      z_pred = z_pred + weights_(i) * Zsig_.col(i);
  }

  //measurement covariance matrix S

  MatrixXd S = MatrixXd(n_z_,n_z_);
  S.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {

    VectorXd z_diff = Zsig_.col(i) - z_pred;

    //angle normalization
    z_diff(1) = NormalizePhi(z_diff(1));



    S = S + weights_(i) * z_diff * z_diff.transpose();
  }

  //add measurement noise covariance matrix
  MatrixXd R = MatrixXd(n_z_,n_z_);
  R <<    std_radr_*std_radr_, 0, 0,
          0, std_radphi_*std_radphi_, 0,
          0, 0,std_radrd_*std_radrd_;
  S = S + R;


  //write result
  *z_out = z_pred;
  *S_out = S;
}


float UKF::NormalizePhi(float angle){

	/**

	  * This method is to normalized the angle, so that it should remain between pi and -pi
	  * Input: take angle as input
	  * output: return the normalized angle

	*/


	if(fabs(angle) > M_PI){
			angle -= round(angle / (2. * M_PI)) * (2.* M_PI);
		}

		return angle;
}

