
/**************************************************************************
 * This file is part of Celera Assembler, a software program that 
 * assembles whole-genome shotgun reads into contigs and scaffolds.
 * Copyright (C) 1999-2004, Applera Corporation. All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received (LICENSE.txt) a copy of the GNU General Public 
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *************************************************************************/

static char CM_ID[] = "$Id: AS_GKP_main.c,v 1.15 2007-02-12 22:16:57 brianwalenz Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "AS_global.h"
#include "AS_PER_genericStore.h"
#include "AS_PER_gkpStore.h"
#include "AS_UTL_PHash.h"
#include "AS_UTL_version.h"
#include "AS_MSG_pmesg.h"
#include "AS_GKP_include.h"

GateKeeperStore *gkpStore;

void
printGKPError(FILE *fout, GKPErrorType type){

  switch(type){
    case GKPError_FirstMessageBAT:
      fprintf(fout,"# GKP Error %d: First message MUST be BAT\n",(int)type);
      break;
    case GKPError_BadUniqueBAT:
      fprintf(fout,"# GKP Error %d: UID of batch definition was previously seen\n",(int)type);
      break;
    case GKPError_BadUniqueFRG:
      fprintf(fout,"# GKP Error %d: UID of fragment definition was previously seen\n",(int)type);
      break;
    case GKPError_BadUniqueLIB:
      fprintf(fout,"# GKP Error %d: UID of library definition was previously seen\n",(int)type);
      break;
    case GKPError_MissingFRG:
      fprintf(fout,"# GKP Error %d: Fragment not previously defined\n",(int)type);
      break;
    case GKPError_MissingLIB:
      fprintf(fout,"# GKP Error %d: Library not previously defined\n",(int)type);
      break;
    case GKPError_DeleteFRG:
      fprintf(fout,"# GKP Error %d: Can't delete Fragment\n",(int)type);
      break;
    case GKPError_DeleteLIB:
      fprintf(fout,"# GKP Error %d: Can't delete Library\n",(int)type);
      break;
    case GKPError_DeleteLNK:
      fprintf(fout,"# GKP Error %d: Can't delete Link\n",(int)type);
      break;
    case GKPError_Time:
      fprintf(fout,"# GKP Error %d: Invalid creation time\n",(int)type);
      break;
    case GKPError_Action:
      fprintf(fout,"# GKP Error %d: Invalid action\n",(int)type);
      break;
    case GKPError_Scalar:
      fprintf(fout,"# GKP Error %d: Invalid scalar\n",(int)type);
      break;
    case GKPError_FRGSequence:
      fprintf(fout,"# GKP Error %d: Invalid fragment sequence characters\n",(int)type);
      break;
    case GKPError_FRGQuality:
      fprintf(fout,"# GKP Error %d: Invalid fragment quality characters\n",(int)type);
      break;
    case GKPError_FRGLength:
      fprintf(fout,"# GKP Error %d: Invalid fragment length\n",(int)type);
      break;
    case GKPError_FRGClrRange:
      fprintf(fout,"# GKP Error %d: Invalid fragment clear range must be 0<=clr1<clr2<=length\n",(int)type);
      break;
    case GKPError_FRGLocalPos:
      fprintf(fout,"# GKP Error %d: Invalid fragment locale pos\n",(int)type);
      break;
    case GKPError_FRGQualityWindow:
      fprintf(fout,"# GKP Error %d: Bad fragment window quality\n",(int)type);
      break;
    case GKPError_FRGQualityGlobal:
      fprintf(fout,"# GKP Error %d: Bad fragment global quality\n",(int)type);
      break;
    case GKPError_FRGQualityTail:
      fprintf(fout,"# GKP Error %d: Bad fragment tail quality\n",(int)type);
      break;
    case GKPError_LNKFragLibMismatch:
      fprintf(fout,"# GKP Error %d: Link fragment library mismatch\n",(int)type);
      break;
    case GKPError_LNKOneLink:
      fprintf(fout,"# GKP Error %d: Violation of unique mate/bacend link per fragment\n",(int)type);
      break;
    case GKPError_DSTValues:
      fprintf(fout,"# GKP Error %d: DST mean,stddev must be >0 and mean must be >= 3 * stddev\n",(int)type);
      break;
    default:
      fprintf(stderr,"#### printGKPError: error type %d\n", (int)type);
      break;
  }
}



static
void
usage(char *filename) {
  fprintf(stderr, "usage: %s [-aiefnopsCGPNQX] <input.frg> <input.frg> ...\n", filename);
  fprintf(stderr, "\n");
  fprintf(stderr, "Opens <Input> to read .frg input\n");
  fprintf(stderr, "Creates GateKeeperFragmentStore in <output_store_name>\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "  -a                     append to existing tore\n");

  fprintf(stderr, "  -e <errorThreshhold>   set error threshhold\n");

  fprintf(stderr, "  -h                     print usage\n");
  fprintf(stderr, "  -H                     print error messages\n");

  fprintf(stderr, "  -o <gkpStore>\n");

  fprintf(stderr, "  -v                     enable verbose mode\n");

  fprintf(stderr, "  -G                     gatekeeper for assembler Grande (default)\n");
  fprintf(stderr, "  -T                     gatekeeper for assembler Grande with Overlap Based Trimming\n");

  fprintf(stderr, "  -Q                     don't check quality-based data quality\n");
}



int
main(int argc, char **argv) {
  int              append          = 0;
  int              outputExists    = 0;
  int              verbose         = 0;
  int              check_qvs       = 1;
  int              assembler       = AS_ASSEMBLER_GRANDE;
  CDS_CID_t        currentBatchID  = NULLINDEX;

  char            *gkpStoreName    = NULL;

  time_t           currentTime     = time(0);

  int              nerrs           = 0;   // Number of errors in current run
  int              maxerrs         = 1;   // Number of errors allowed before we punt

  int              firstFileArg    = 0;

  int arg = 1;
  int err = 0;
  while (arg < argc) {
    if        (strcmp(argv[arg], "-a") == 0) {
      append = 1;
    } else if (strcmp(argv[arg], "-e") == 0) {
      maxerrs = atoi(argv[++arg]);
    } else if (strcmp(argv[arg], "-h") == 0) {
      err++;
    } else if (strcmp(argv[arg], "-H") == 0) {
      int i;
      fprintf(stderr,"The following failures are detected:\n");
      for(i=1; i<=MAX_GKPERROR; i++)
        printGKPError(stderr, (GKPErrorType)i);
      exit(0);
    } else if (strcmp(argv[arg], "-o") == 0) {
      gkpStoreName = argv[++arg];

    } else if (strcmp(argv[arg], "-v") == 0) {
      verbose = 1;

    } else if (strcmp(argv[arg], "-G") == 0) {
      assembler = AS_ASSEMBLER_GRANDE;
    } else if (strcmp(argv[arg], "-T") == 0) {
      assembler = AS_ASSEMBLER_OBT;
    } else if (strcmp(argv[arg], "-Q") == 0) {
      check_qvs = 0;
    } else if (strcmp(argv[arg], "--") == 0) {
      firstFileArg = arg++;
      arg = argc;
    } else if (argv[arg][0] == '-') {
      fprintf(stderr, "unknown option '%s'\n", argv[arg]);
      err++;
    } else {
      firstFileArg = arg;
      arg = argc;
    }

    arg++;
  }
  if ((err) || (gkpStoreName == NULL) || (firstFileArg == 0)) {
    usage(argv[0]);
    exit(1);
  }


  outputExists = testOpenGateKeeperStore(gkpStoreName, FALSE);

  if ((append == 0) && (outputExists == 1)) {
    fprintf(stderr,"* Gatekeeper Store %s exists and append flag not supplied.  Exit.\n", gkpStoreName);
    exit(1);
  }
  if ((append == 1) && (outputExists == 0)) {
    //  Silently switch over to create
    append = 0;
  }
  if ((append == 0) && (outputExists == 1)) {
    fprintf(stderr,"* Gatekeeper Store %s exists, but not told to append.  Exit.\n", gkpStoreName);
    exit(1);
  }

  if (append)
    gkpStore = openGateKeeperStore(gkpStoreName, TRUE);
  else
    gkpStore = createGateKeeperStore(gkpStoreName);

  currentBatchID = getNumGateKeeperBatchs(gkpStore->bat);

  for (; firstFileArg < argc; firstFileArg++) {
    FILE            *inFile       = NULL;
    GenericMesg     *pmesg        = NULL;
    int              messageCount = 0;

    errno = 0;
    inFile = fopen(argv[firstFileArg], "r");
    if (errno) {
      fprintf(stderr, "%s: failed to open input '%s': %s\n", argv[0], argv[firstFileArg], strerror(errno));
      exit(1);
    }

    while (EOF != ReadProtoMesg_AS(inFile, &pmesg)) {
      messageCount++;

      if (((messageCount == 1) && (pmesg->t != MESG_BAT)) ||
          ((messageCount != 1) && (pmesg->t == MESG_BAT))) {
        printGKPError(stderr, GKPError_FirstMessageBAT);
        WriteProtoMesg_AS(stderr,pmesg);
        return GATEKEEPER_FAILURE;
      }

      if (pmesg->t == MESG_BAT) {
        if (GATEKEEPER_SUCCESS != Check_BatchMesg((BatchMesg *)pmesg->m, &currentBatchID, currentTime, verbose)) {
          fprintf(stderr,"# Invalid BAT message at Line %d of input...exiting\n", GetProtoLineNum_AS());
          WriteProtoMesg_AS(stderr,pmesg);
          return GATEKEEPER_FAILURE;
        }
	
      } else if (pmesg->t == MESG_DST) {
        if (GATEKEEPER_SUCCESS != Check_LibraryMesg((DistanceMesg *)pmesg->m, currentBatchID, verbose)){
          fprintf(stderr,"# Line %d of input\n", GetProtoLineNum_AS());
          WriteProtoMesg_AS(stderr,pmesg);
          nerrs++;
        }

      } else if (pmesg->t == MESG_FRG) {
        if (GATEKEEPER_SUCCESS != Check_FragMesg((FragMesg *)pmesg->m, check_qvs, currentBatchID, currentTime, assembler, verbose)){
          fprintf(stderr,"# Line %d of input\n", GetProtoLineNum_AS());
          WriteProtoMesg_AS(stderr,pmesg);
          nerrs++;
        }

      } else if (pmesg->t == MESG_LKG) {
        if (GATEKEEPER_SUCCESS != Check_LinkMesg((LinkMesg *)pmesg->m, currentBatchID, currentTime, verbose)) {
          fprintf(stderr,"# Line %d of input\n", GetProtoLineNum_AS());
          WriteProtoMesg_AS(stderr,pmesg);
          nerrs++;
        }

      } else {
        fprintf(stderr,"# ERROR: Read Message with type %s...skipping!\n", MessageTypeName[pmesg->t]);
        WriteProtoMesg_AS(stderr,pmesg);
        nerrs++;
      }

      if (nerrs >= maxerrs) {
        fprintf(stderr, "GateKeeper: max allowed errors reached %d > %d...bye\n", nerrs, maxerrs);
        return(GATEKEEPER_FAILURE);
      }
    }

    fclose(inFile);
  }

  closeGateKeeperStore(gkpStore);

  exit(0);
}
