// This file is part of "LearningMeshSegmentation".

// Copyright (c) 2010 Evangelos Kalogerakis
// All rights reserved. 
// If you use this code or its parts, please also cite 
// "Learning 3D Mesh Segmentation and Labeling, 
// E. Kalogerakis, A. Hertzmann, K. Singh, ACM Transactions on Graphics, 
// Vol. 29, No. 3, July 2010"
// AND (Jointboost was introduced by)
// Antonio Torralba , Kevin P. Murphy , William T. Freeman, Sharing Visual Features 
// for Multiclass and Multiview Object Detection, IEEE Transactions on Pattern Analysis 
// and Machine Intelligence, v.29 n.5, p.854-869, May 2007

// "LearningMeshSegmentation" is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// "LearningMeshSegmentation" is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with "LearningMeshSegmentation".  If not, see <http://www.gnu.org/licenses/>.

// revised a few times after submission.

#include "MeshSegmentationFeatures.h"
#include "JointBoost.h"


// C^2 Greedy algorithm

void JointBoost::train2( int maxrounds ) {
	std::cout << "Initializing Jointboost - GREEDY SEARCH MODE ACTIVATED!..." << std::endl;	
	std::cout.flush();
	selfDestruct();
	trainMode = true;
	testMode = false;

	if (XC != NULL) {
		HC = new float*[ numCVExamples ];
		for (int i = 0; i < numCVExamples; i++) {
			HC[i] = new float[ numClasses ];
			for (int c = 0; c < numClasses; c++) {
				HC[i][c] = 0.0f;
			}
		}
	}
	PH = new float*[ numExamples ];
	for (int i = 0; i < numExamples; i++) {
		PH[i] = new float[ numClasses ];
	}
	YP = new int[ numExamples ];

	W = new float*[ numExamples ];
	H = new float*[ numExamples ];
	for (int i = 0; i < numExamples; i++) {
		W[i] = new float[ numClasses ];
		H[i] = new float[ numClasses ];
		for (int c = 0; c < numClasses; c++) {
			W[i][c] = WI[i];
			H[i][c] = 0.0f;
		}
	}
	SI = new unsigned int*[ numFeatures ];
	for (int f = 0; f < numFeatures; f++) {
		SI[f] = sortArrayIndices( X[f], numExamples );
	}
  cumsumSubSample = (unsigned int)ceil((4.0f * float(numFeatures) * float(numClasses) * float(numExamples)) / ((float)MAX_SIZE_ARRAY_BYTES));
	cumsumSize = (unsigned int)ceil( (float)numExamples / (float)cumsumSubSample );
	std::cout << "cumsumSubSample = " << cumsumSubSample << ", cumsumSize=" << cumsumSize << ", numFeatures=" << numFeatures << std::endl;
	cumsumYW = new float**[numFeatures];
	cumsumW = new float**[numFeatures];
	for (int f = 0; f < numFeatures; f++) {
		cumsumYW[f] = new float*[numClasses];
		cumsumW[f] = new float*[numClasses];
		for (int c = 0; c < numClasses; c++) {
			cumsumYW[f][c] = new float[ cumsumSize ];
			cumsumW[f][c] = new float[ cumsumSize ];
		}
	}	

	maxRounds = maxrounds;
	int maxn = 1 << numClasses;
	bool *S = new bool[ numClasses ];
	bool *bestS = new bool[ numClasses ];
	float *kc = new float[ numClasses ];
	SharedStump* bestStump = NULL;
	bestStumps = new SharedStump*[ maxRounds ] ;
	SharedStump* currentStump = new SharedStump(numClasses);	
	int round = 0;
	float CVerror = getCVError(), trainingError = getTrainingError();
	minCVerror = FLT_MAX;
	int roundsWithLargerCVErrorThanMin = 0;
	int bestRound = 0;
	std::cout << "In the beginning: Training Error: " << trainingError << ", Validation Error: " << CVerror << std::endl;


	for (round = 1; round <= maxrounds; round++) {
		for (int f = 0; f < numFeatures; f++) {
			for (int c = 0; c < numClasses; c++) {


				int i = 0;
				float sumYW_i = 0.0f, sumW_i = 0.0f;
				float sumYW_old = 0.0f, sumW_old = 0.0f;
				while (i < numExamples) {
					int ii = i;
					int pos = SI[f][ii], prevpos;
					sumYW_i = labelToBin(Y[pos], c+1) * W[pos][c];
					sumW_i  =  W[pos][c];
					ii++;
					while (ii < numExamples) {
						pos = SI[f][ii];
						prevpos = SI[f][ii-1];
						if ( X[f][pos] == X[f][prevpos] ) {
							sumYW_i += labelToBin(Y[pos], c+1) * W[pos][c];
							sumW_i +=  W[pos][c];
						} else {
							break;
						}
						ii++;
					}
					int nexti = ii;
					ii--;
					while (ii >= i) {
						if (ii % cumsumSubSample == 0) {
							cumsumYW[f][c][ii / cumsumSubSample] = sumYW_old + sumYW_i;
							cumsumW[f][c][ii / cumsumSubSample] = sumW_old + sumW_i;
						}
						ii--;
					}
					sumYW_old += sumYW_i;
					sumW_old += sumW_i;
					i = nexti;
				}


				//cumsumYW[f][c][0] = labelToBin(Y[ SI[f][0] ], c+1) * W[SI[f][0]][c];
				//cumsumW[f][c][0] = W[SI[f][0]][c];
				//for (int i = 1; i < cumsumSize; i++) {
				//	float tmpsumYW = 0.0f, tmpsumW = 0.0f;
				//	for (int ii = 1; ii <= cumsumSubSample; ii++) {
				//		tmpsumYW += labelToBin(Y[ SI[f][(i-1)*cumsumSubSample+ii] ], c+1) * W[SI[f][(i-1)*cumsumSubSample+ii]][c];
				//		tmpsumW +=  W[SI[f][(i-1)*cumsumSubSample+ii]][c];
				//	}
				//	cumsumYW[f][c][i] = tmpsumYW + cumsumYW[f][c][i-1];
				//	cumsumW[f][c][i] = tmpsumW + cumsumW[f][c][i-1];
				//}

			}

		}	


		float minError = FLT_MAX;
		for (int c = 0; c < numClasses; c++) {
			bestS[c] = false;
		}

		for (int ci = 0; ci < numClasses; ci++) {
			int bestc_to_add = -1;
			for (int c = 0; c < numClasses; c++) {
				if (bestS[c])
					continue;
				for (int cj = 0; cj < numClasses; cj++) {
					S[cj] = bestS[cj];				
				}
				S[c] = true;
				float thirdTermErr = 0.0f;
				for (int cj = 0; cj < numClasses; cj++) {
					kc[cj] = cumsumYW[0][cj][cumsumSize-1] / cumsumW[0][cj][cumsumSize-1];
					if (S[cj] == false) {
						for (int i = 0; i < numExamples; i++) {
							float thirdTermErrFactor =  labelToBin(Y[i], cj+1) - kc[cj];
							thirdTermErr += W[i][cj] * thirdTermErrFactor * thirdTermErrFactor;
						}
					}
				}
				for (int f = 0; f < numFeatures; f++) {
					currentStump->update( S, f, kc );
					fitStump(currentStump, thirdTermErr);
					float err = getError(currentStump);
					if ( err <= minError ) {
						if (bestStump != NULL) {
							delete bestStump;
						}
						minError = err;
						bestStump = new SharedStump(currentStump);
						bestc_to_add = c;
					}
				}
			}

			if (bestc_to_add != -1)
				bestS[bestc_to_add] = true;
		}

////////////
////////////
////////////

////////////////////////
////////////
////////////
////////////


		updateLearner( bestStump );
		bestStumps[round-1] = new SharedStump(bestStump);
		// delete bestStump;
		trainingError = getTrainingError();
		if (XC != NULL) {
			updateCVLearner( bestStump );
			CVerror = getCVError();
			if (CVerror < minCVerror) {
				minCVerror = CVerror;
				roundsWithLargerCVErrorThanMin = 0;
				bestRound = round;
			} else {
				if ( roundsWithLargerCVErrorThanMin++ >= STOP_AFTER_N_ROUNDS_OF_CVERROR_INCREASING )
					break;
			}
		} else {
			CVerror = -0;
			minCVerror = -0;
			bestRound = round;
		}

		std::cout << "Round " << round << " done. Training Error: " << trainingError << ", Validation Error: " << CVerror << ", RWLTM=" <<  roundsWithLargerCVErrorThanMin << ", feat=" << bestStump->f << std::endl;
		std::cout.flush();
	}

	std::cout << "*** Best Round was " << bestRound << ". CV Error: " << minCVerror << std::endl;
	std::cout.flush();

	// after bestRound is select, training prediction must be re-evaluated
	maxRounds = bestRound;
	for (int i = 0; i < numExamples; i++) {
		for (int c = 0; c < numClasses; c++) {
			H[i][c] = 0.0f;
			W[i][c] = WI[i];
		}
	}
	for (int round = 1; round <= maxRounds; round++) {
		updateLearner( bestStumps[round-1] );
	}
	predict();

	delete[] S;
	delete[] bestS;
	delete[] kc;
	delete currentStump;
}

