/*
Author: Minsheng Zhang
    Shanshan Gao
*/

#include "parserInput.hpp"
#include "tileToSetCover.hpp"
#include "dancinglink.hpp"
#include <iostream>
#include <vector>
#include "def.h"
#include <algorithm>
#include <mpi.h>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <stack>

using namespace std;
int dataset[10000000];
int metadata[MAX_PROC_NUMBER * 2];
int totalsubset = 0;
int totalSolutions = 0;
int resultPos[2];
int resultSet[10000000];

DL *dl = NULL;
TileToSC *sc = NULL;
vector <vector<int> > matrix;

void scatterMetaData(int *data, int *metadata, int metadatasize, int P, int p);

void
scatterMetaData(int *data,
        int *metadata,
        int metadatasize,
        int P,
        int p)
{
  int pN = P,
    idx = 0,
    tag = 101,
    rcvMetadatasize,
    datasize,
    newN;

  bool  isMaster = (p == 0);

  MPI_Status  status;

  if(isMaster){
    newN = (int) ceil(metadatasize*1.0/pN);
    for(int i=0;i<pN;i++){
      if((i*newN)>=metadatasize){
        MPI_Send(metadata+metadatasize*2,2,MPI_INTEGER,idx+i,tag,MPI_COMM_WORLD);
      }else{
        if(newN*(i+1) <= metadatasize)
          metadata[newN*2*i+1] = metadata[newN*2*(i+1)-1];
        else
          metadata[newN*2*i+1] = metadata[metadatasize*2-1];
        if(i!=0){
          datasize = metadata[newN*2*i+1]-metadata[newN*2*i]+1;
          MPI_Send(metadata+newN*2*i,2,MPI_INTEGER,idx+i,tag,MPI_COMM_WORLD);
          MPI_Send(data+metadata[newN*2*i],datasize,MPI_INTEGER, idx+i, tag, MPI_COMM_WORLD);
        }
      }
    }
  }else{
    rcvMetadatasize = 2;

    MPI_Recv(metadata, rcvMetadatasize, MPI_INTEGER, idx, tag, MPI_COMM_WORLD, &status);
    datasize = 1+metadata[1]-metadata[0];

    if(metadata[0]==metadata[1] && metadata[0]==0)
      ;
    else
      MPI_Recv(data, datasize, MPI_INTEGER, idx, tag, MPI_COMM_WORLD, &status);
  }
}

void
scatterResult(int *pos, int *data, int p, int NP)
{
  int idx = 0,
      tag = 101,
      datasize,
      offset;

  bool  isMaster = (p == 0);

  MPI_Status  status;

  if(!isMaster) {
    MPI_Send(pos,2, MPI_INTEGER, idx, tag, MPI_COMM_WORLD);
    datasize = pos[1]-pos[0]+1;
    if(datasize ==0)
      return;
    MPI_Send(data,datasize, MPI_INTEGER, idx, tag, MPI_COMM_WORLD);
  } else {
    for(int i=0;i<NP-1;i++){
      int a[2];
      MPI_Recv(a, 2, MPI_INTEGER, i+1, tag, MPI_COMM_WORLD, &status);
  
      if(a[1]-a[0]<=0)
        continue;
      else{
        datasize = a[1] - a[0] + 1;
        MPI_Recv(data+pos[1]+1,datasize, MPI_INTEGER, i+1, tag, MPI_COMM_WORLD, &status);
        pos[1] = pos[1] + datasize;
      }
    } 
  }
}

int main(int argc, char* argv[])
{
  int initExploreLevel = 0;

  double  stime = 0.0,
          seqetime = 0.0,
          etime = 0.0,
          mytime = 0.0;

  int size;
  MPI_Request request;
  MPI_Status  status;

  int id,
  ierr,
  tag = 100;
  
  /* Initialize MPI */
  ierr = MPI_Init(&argc, &argv);
  ierr = MPI_Comm_size(MPI_COMM_WORLD, &size);
  ierr = MPI_Comm_rank(MPI_COMM_WORLD, &id);
  /* Initialization finishes */

  int MASTER = (id == 0);
  
  stime = MPI_Wtime();

  resultPos[0] = 0; resultPos[1] = -1;
  if(MASTER)
  {
    inputParser ip; 
    ip.startParsers(argv[1]);

    vector<string> board = ip.getBoard();
    vector <vector<string> > pieces = ip.getPieces();

    cout << "Start Convert to Exact Cover.." << endl;
    sc = new TileToSC(pieces,board);
    sc->startConvert();
    matrix = sc->getSet();
  
    cout << "Finish Convert.." << endl;
    cout << "Start Master Process to search the result.."<< endl;
    dl = new DL(matrix);    
    dl->search(0,0);
    cout << "Ending the Master process search.." << endl;
  }

  seqetime = MPI_Wtime();
  
  scatterMetaData(dataset, metadata, totalsubset, size, id);

  if(MASTER)
    delete dl;

  for(int i = 0; i < metadata[1]-metadata[0]+1; i++)
  {
    vector <vector<int> > subMatrix;

    int totalPartialResult = dataset[i];
    if(totalPartialResult==0) break;
    vector <int> partial;

    for(int j=0;j<totalPartialResult;j++)
      partial.push_back(dataset[i+j+1]);

    sort(partial.begin(),partial.end());

    int rows = dataset[i+totalPartialResult+1];
    int cols = dataset[i+totalPartialResult+2];
    vector<int> allrows;
    int rowsize;

    for(int j=0;j<cols;j++)
      for(int k=0;k<rows;k++)
      {
        int temp = dataset[i+totalPartialResult+3+j*rows+k];
        if(j==0) 
        {
          vector<int> tempv;
          tempv.push_back(temp);
          subMatrix.push_back(tempv);
        }
        else
        {
          subMatrix[k].push_back(temp);
        }
      }

    rowsize = dataset[i+totalPartialResult+rows*cols+3];
    for(int j=0;j<rowsize;j++){
      allrows.push_back(dataset[i+totalPartialResult+4+rows*cols+j]);
    }
    sort(allrows.begin(),allrows.end());

    i += 1+totalPartialResult+2+rows*cols+allrows.size();
  
    if(subMatrix.size()==0 && subMatrix[0].size()==0){
      ;
    }else if(subMatrix.size()!=0&&subMatrix[0].size()!=0){
      dl = new DL(subMatrix);
      for(int ii=0;ii<partial.size();ii++)
      {
        dl->pushPartial(partial[ii]);
      }
      dl->checkRows(allrows);
      dl->search(0);
      delete dl;
    }
  }

  scatterResult(resultPos,resultSet,id,size);

  int grandTotalSolutions = 0;
  MPI_Reduce(&totalSolutions, &grandTotalSolutions, 1, MPI_INTEGER, MPI_SUM, 0, MPI_COMM_WORLD);
  
  etime = MPI_Wtime();
  mytime = etime - stime;

  if(MASTER)
  {
    cout << "There are " << min(grandTotalSolutions, sc->getDiffResult(resultSet,resultPos[1]-resultPos[0]+1,matrix)) << " different result." << endl;
  }

  if (MASTER)
  {
    cout << "Total Execuation time:  " << mytime << " second(s)\n";
  }

  MPI_Finalize();
  return 0;
}
