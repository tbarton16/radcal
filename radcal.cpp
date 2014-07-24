/*H**********************************************************************
* FILENAME :        radcal.cpp
*
* DESCRIPTION :
*       Takes in matrix of weights as input. Determines top n% of weights to
b*e used as points of no change for radiometric normalization. Using the (x,y
) coordinates of these
*
* PUBLIC FUNCTIONS :
*       type    FunctionName( Parameter1, Parameter2 )
*
* NOTES :
*
*
*       Copyright (c) Mort Cantey(2007), Theresa Barton(2014).
*
* CHANGES :
*
*
*H*/

/**************************************************************************
*   INCLUDE FILES
***************************************************************************/
// to compile g++ -I /usr/local/Cellar/eigen/3.2.1/include/eigen3 radcal.cpp GdalFileIO.cpp -lgdal

#include "gdal_priv.h"
#include "cpl_conv.h" // for CPLMalloc()
#include "stdio.h"
#include "time.h"
#include "radcal.h"
#include "tgmath.h"
// header files from Donovan Parks
#include "Precompiled.hpp"
#include "LinearRegression.hpp"

#include <ogr_spatialref.h>
#include <string>
#include <vector>
#include <iostream>
#include <Eigen/Dense>


#include <cmath>
#include <algorithm>

/**************************************************************************
*   NameSpaceIng
***************************************************************************/

using namespace std;
using Eigen::MatrixXd;//this represents a matrix of arbitrary size (hence the X in MatrixXd), in which every entry is a doubl
using Eigen::VectorXd;



/**************************************************************************
*   Data points input
***************************************************************************/
vector<pair<int,int> > * xyget(){

   vector<pair<int,int> > * generatedpoints = new vector<pair<int,int> >();
   generatedpoints->push_back(make_pair(1,1));
   generatedpoints->push_back(make_pair(9733	,11170	));
   generatedpoints->push_back(make_pair(10946, 3574));

   generatedpoints->push_back(make_pair(13862,12724));
   generatedpoints->push_back(make_pair(6520,10070));
   generatedpoints->push_back(make_pair(16168, 13875));
   generatedpoints->push_back(make_pair(8449, 5886));
   generatedpoints->push_back(make_pair(479, 6309));
   generatedpoints->push_back(make_pair(1213,5256));
   generatedpoints->push_back(make_pair(10283,1856));
   return generatedpoints;

}

/**************************************************************************
*   Linear Regression Code (from Donovan Parks)
***************************************************************************/

bool LinearRegression::LeastSquaresEstimate(std::vector<double>& x,
std::vector<double>& y, RESULTS& results)// needs to return vector of gain and offset information
{
   int numPts = x.size();
   if(numPts < 2)
      return false;

      double sumX = 0;
      double sumY = 0;
      double sumXY = 0;
      double sumX2 = 0;
      double sumY2 = 0;
      for(int i=0; i < numPts; ++i)
         {
            sumX += x.at(i);
            sumY += y.at(i);
            sumXY	+= x.at(i) * y.at(i);
            sumX2	+= x.at(i) * x.at(i);
            sumY2	+= y.at(i) * y.at(i);
         }

         double SSxy = sumXY - (sumX*sumY) / numPts;
         double SSxx = sumX2 - (sumX*sumX) / numPts;
         double SSyy = sumY2 - (sumY*sumY) / numPts;

         if(SSxx != 0 && SSyy != 0)
            {
               results.slope = SSxy / SSxx;

               results.cod = (SSxy*SSxy) / (SSxx*SSyy);
               results.r = SSxy / sqrt(SSxx*SSyy);
            }
            else if(SSxx == 0 && SSyy == 0)
            {
               results.slope = DBL_MAX;
               results.cod = DBL_MAX;
               results.r = 1;
            }
         else
            {
               results.slope = DBL_MAX;

               results.cod = 1;
               results.r = -1;
            }

            results.offset = (sumY - results.slope*sumX) / numPts;
            results.dof = numPts-2;
            results.nPts = numPts;

            double SSE = SSyy-results.slope*SSxy;
            results.sd = sqrt(SSE / results.dof);
            results.seos = results.sd / sqrt(SSxx);
            results.seoi = results.sd*sqrt((1/numPts) + ((sumX/numPts)*(sumX/numPts))/SSxx);

            results.tStat = results.slope / results.seos;

            return true;
         }

         /**
         * Calculate the error of a given best fit line.
         */
         double LinearRegression::Error(int numPts, double sumX, double sumY, double sumXY,
         double sumX2, double sumY2)
         {
            double SSxx = sumX2 - (sumX*sumX) / numPts;
            double SSyy = sumY2 - (sumY*sumY) / numPts;
            double SSxy = sumXY - (sumX*sumY) / numPts;

            double slope = SSxy / SSxx;
            double offset = (sumY - slope*sumX) / numPts;

            return SSyy - slope*SSxy;
         }

         /**************************************************************************
         *   Radcal
         ***************************************************************************/
         void radcal(){

            GDALAllRegister();
            OGRSpatialReference oSRS;
            // inputs
            string filename="tjpeg.tif";
            string filenameagain = "tjpeg.tif";
            //extract bands at (x,y)
            GDALDataset* file1 = GdalFileIO::openFile(filename);
            GDALDataset* file2 = GdalFileIO::openFile(filenameagain);

            vector<pair<int,int> > *generatedpoints = xyget();

            int   nXSize = file1->GetRasterXSize();
            int   nYSize = file1-> GetRasterYSize();
            int numpoints =generatedpoints->size();
            int numbands = file1->GetRasterCount();
            MatrixXd pixValsMat(numbands,numpoints); // need to change to doubles
            GDALRasterBand *rasterband;
            int nBlockXSize, nBlockYSize;

            for( int band=1;band<numbands+1;band++){
               //cout << "_____" << "band" << i << "____"<<"\n";
               for(int point = 0;point<numpoints; point++){
                  rasterband = file1->GetRasterBand(band);
                  int pixel= generatedpoints-> at(point).first;
                  int line = generatedpoints-> at(point).second;
                  int nXSize = rasterband->GetXSize();
                  double * value;
                  value = (double *) CPLMalloc(sizeof(double)*nXSize);
                  rasterband->RasterIO( GF_Read, pixel, line, 1, 1,value, 1 /*nXSize*/, 1, GDT_Float64,
                  0, 0 );
                  pixValsMat(band-1, point)=*value;
                  //cout << *value<< "\n";

                  CPLFree(value);
               }
            }
            MatrixXd pixValsMat2(numbands,numpoints);
            for( int band=1;band<numbands+1;band++){
               //cout << "_____" << "band" << i << "____"<<"\n";
               for(int point = 0;point<numpoints; point++){
                  rasterband = file2->GetRasterBand(band);
                  int pixel= generatedpoints-> at(point).first;
                  int line = generatedpoints-> at(point).second;
                  int nXSize = rasterband->GetXSize();
                  double * value;
                  value = (double *) CPLMalloc(sizeof(double)*nXSize);
                  rasterband->RasterIO( GF_Read, pixel, line, 1, 1,value, 1 /*nXSize*/, 1, GDT_Float64,
                  0, 0 );
                  pixValsMat2(band-1, point)=*value;
                  //cout << *value<< "\n";

                  CPLFree(value);
               }
            }
            // for(int x=0;x<numbands;x++)  // loop 3 times for three lines
            //    {
            //       for(int y=0;y<numpoints;y++)  // loop for the three elements on the line
            //          {
            //             cout<<pixValsMat(x,y)<<" ";  // display the current element out of the array
            //          }
            //          cout<<endl;  // when the inner loop is done, go to a new line
            //       }

            /*run Regression*/
            MatrixXd lrparams(numbands,2);

            // vectors are rows, pass vectors to linreg
            for(int band =0; band < numbands; band++){
            LinearRegression lin =LinearRegression();
            double pixValsArr [numpoints];
            for( int pixel = 0; pixel <numpoints; pixel++){
               pixValsArr[pixel] = pixValsMat(band,pixel);
            }
            vector<double> pixValsVec (pixValsArr, pixValsArr + sizeof(pixValsArr) / sizeof(double) );

            double pixValsArr2[numpoints];
            for( int pixel = 0; pixel <numpoints; pixel++){
               pixValsArr2[pixel] = pixValsMat2(band,pixel);
            }
            vector<double> pixValsVec2 (pixValsArr2, pixValsArr2 + sizeof(pixValsArr2) / sizeof(double) );
            LinearRegression::RESULTS results = LinearRegression::RESULTS();
            lin.LeastSquaresEstimate(pixValsVec, pixValsVec2,results);//member function
            double gain = results.slope;
            cout << "gain for band n : " <<gain <<"\n";
            double offset = results.offset;
            cout << "offset for band n : " <<offset <<"\n";
            lrparams(band,0)=gain;
            lrparams(band,1)=offset;

            }
            for(int x=0;x<numbands;x++)  // loop 3 times for three lines
               {
                  for(int y=0;y<2;y++)  // loop for the three elements on the line
                     {
                        cout<<lrparams(x,y)<<" ";  // display the current element out of the array
                     }
                     cout<<endl;  // when the inner loop is done, go to a new line
                  }

            GDALClose(file1);
            GDALClose(file2);

         }

         int main()
         {

            radcal();
            return 0;
         }
