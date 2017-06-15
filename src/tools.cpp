#include <iostream>
#include "tools.h"

using Eigen::VectorXd;
using Eigen::MatrixXd;
using std::vector;

Tools::Tools() {}

Tools::~Tools() {}

VectorXd Tools::CalculateRMSE(const vector<VectorXd> &estimations,
                              const vector<VectorXd> &ground_truth) {
  /**

    Input:
     * Vector reference of estimations
     * Vector reference of ground_truth
    Output:
     * Return Vector having RMSE value of Px, Py, Vx, and Vy
  */

    VectorXd rmse(4);

    rmse.fill(0);
    if(estimations.size() != ground_truth.size() || estimations.size() == 0  )
    {
        cout <<"Error: invalid input" <<endl;
        return rmse;
    }

    //accumulate squared error
    for(int i=0; i < estimations.size(); ++i){

    VectorXd e = estimations[i] - ground_truth[i];
    e = e.array()*e.array();
    rmse +=e;

    }

    //calculate the mean

    rmse = rmse/estimations.size();

    //calculate the squared root

    rmse = rmse.array().sqrt();

    //return the result
    return rmse;

}
