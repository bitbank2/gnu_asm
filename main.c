//
// gnu_asm - convert ARMASM source files into GNU ASM syntax
// Written by Larry Bank
// Copyright (c) 2010-2017 BitBank Software, Inc.
// Change history
// 11/14/10 - Started the project
// 5/27/17 - Converted to Linux
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

// Specific actions taken on the file:
// 1) Change name from *.ASM to *.S
// 2) Convert ";" to "@" for comments
// 3) Add ":" after labels

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "emuio.h"

void *ihandle, *ohandle;
#ifndef FALSE
typedef int BOOL;
#define FALSE 0
#define TRUE 1
#endif

int PreParseLine(char *pSrc, int *iOff, int iSize, char *pDest)
{
int iOffset = *iOff;
int iStart, i, j;
int iLen = 0;

   iStart = iOffset; // start of this line
   // find end of line to determine length
   while (iOffset < iSize && pSrc[iOffset] != 0xa && pSrc[iOffset] != 0xd)
      {
      iLen++;
      iOffset++;
      }
   // exclude both CR and LF
   while (iOffset < iSize && (pSrc[iOffset] == 0xa || pSrc[iOffset] == 0xd))
      {
      iOffset++;
      }
   // we've captured a line, now filter it
   *iOff = iOffset; // return new caret
   j = 0; // output length
   for (i=0; i<iLen; i++)
      {
      char c = pSrc[iStart+i];
      if (c == ';')
         {
         pDest[j++] = '@'; // convert spaces
         }
      else if (c == 0x9) // convert tabs to spaces
         {
         pDest[j++] = ' '; // insert 4 spaces for a tab
         pDest[j++] = ' ';
         pDest[j++] = ' ';
         pDest[j++] = ' ';
         }
      else if (c == '$') // eat $ chars
         {
         }
      else // all other characters
         {
         pDest[j++] = c;
         }
      }
   pDest[j] = '\0';
   return j;
} /* PreParseLine() */

void Tokenize(char *pSrc, int iLen, char *szLabel, char *szKeyword, char *szOpcode, char *szComment)
{
int i = 0;
int j;

   szLabel[0] = '\0';
   szKeyword[0] = '\0';
   szOpcode[0] = '\0';
   szComment[0] = '\0';
   
   if (pSrc[0] >= '@') // starts with a label
     {
     j = 0;
     while (i < iLen && pSrc[i] != ' ')
        {
        szLabel[j++] = pSrc[i++];
        }
     szLabel[j] = '\0'; // zero terminate the label
     }
   if (i == iLen) return;
   while (i < iLen && pSrc[i] == ' ') // skip forward to next token
      {
      i++;
      }
   // capture keyword
   j = 0;
   while (i < iLen && pSrc[i] != ' ')
      {
      szKeyword[j++] = pSrc[i++];
      }
   szKeyword[j] = '\0';
   
   if (i == iLen) return;
   while (i < iLen && pSrc[i] == ' ') // skip forward to next token
      {
      i++;
      }
   // capture the opcode
   j = 0;
   while (i < iLen && pSrc[i] != ' ')
      {
      szOpcode[j++] = pSrc[i++];
      }
   szOpcode[j] = '\0';
   if (i == iLen) return;
   while (i < iLen && pSrc[i] == ' ') // skip forward to next token
      {
      i++;
      }
   j = 0;
   while (i < iLen) // capture "comment"
      {
      szComment[j++] = pSrc[i++];
      }
   szComment[j] = '\0'; // zero terminate it
} /* Tokenize() */

int ParseLine(char *pLine, int iLen, BOOL bMacro)
{
int iLineLen = 0; // output length
char Label[256], Keyword[256], Opcode[256], Comment[256];
BOOL bReconstruct = FALSE;

   Tokenize(pLine, iLen, Label, Keyword, Opcode, Comment);
   if (bMacro) // the previous line was "MACRO", fix it
      {
      sprintf(pLine, ".macro %s %s %s", Keyword, Opcode, Comment);
      return strlen(pLine);
      }
   if (Label[0] == '@') // whole line of comment, leave
      {
      return iLen; // no changes
      }
   if (Label[0] == '\0') // cases of lines which start with a space
      {
      if (strcasecmp(Keyword, "area") == 0 || strcasecmp(Keyword, "endp") == 0) // toss the whole line out
         {
         return 0;
         }
      else if (strcasecmp(Keyword, "proc") == 0)
         {
         bReconstruct = TRUE;
         Keyword[0] = '\0'; // get rid of "proc"
         }
      else if (strcasecmp(Keyword, "import") == 0)
         {
         bReconstruct = TRUE;
         strcpy(Keyword, ".extern");
         }
      else if (strcasecmp(Keyword, "export") == 0)
         {
         bReconstruct = TRUE;
         strcpy(Keyword, ".global");
         }
      else if (strcasecmp(Keyword, "ltorg") == 0)
         {
         bReconstruct = TRUE;
         strcpy(Keyword, ".ltorg");
         }
      else if (strcasecmp(Keyword, "dcb") == 0)
         {
         bReconstruct = TRUE;
         strcpy(Keyword, ".byte");
         }
      else if (strcasecmp(Keyword, "dcw") == 0)
         {
         bReconstruct = TRUE;
         strcpy(Keyword, ".hword");
         }
      else if (strcasecmp(Keyword, "dcd") == 0)
         {
         bReconstruct = TRUE;
         strcpy(Keyword, ".word");
         }
      else if (strcasecmp(Keyword, "end") == 0)
         {
         bReconstruct = TRUE;
         strcpy(Keyword, ".end");
         }
      else if (strcasecmp(Keyword, "mend") == 0)
         {
         bReconstruct = TRUE;
         strcpy(Label, ".endm");
         Keyword[0] = '\0';
         }
      else if (strcasecmp(Keyword, "if") == 0)
         {
         bReconstruct = TRUE;
         strcpy(Keyword, ".if");
         }
      else if (strcasecmp(Keyword, "endif") == 0)
         {
         bReconstruct = TRUE;
         strcpy(Keyword, ".endif");
         }
      if (bReconstruct)
         {
         sprintf(pLine, "%s  %s  %s  %s", Label, Keyword, Opcode, Comment);
         return strlen(pLine);
         }
      else
         {
         return iLen; // leave the line unchanged
         }
      }
   else // lines which start with a non-space
      {
      if (strcasecmp(Keyword, "equ") == 0)
         { // constants - rework it
         sprintf(pLine, ".equ %s, %s %s", Label, Opcode, Comment);
         return strlen(pLine);
         }
      else if (strcasecmp(Keyword, "rn") == 0) // register assignment
         { // fix the line
         sprintf(pLine, "%s .req r%s %s", Label, Opcode, Comment);
         return strlen(pLine);
         }
      else if (strcasecmp(Keyword, "proc") == 0)
         {
         Keyword[0] = '\0'; // get rid of "proc"
         }
      else if (strcasecmp(Keyword, "dcb") == 0)
         {
         strcpy(Keyword, ".byte");
         }
      else if (strcasecmp(Keyword, "dcw") == 0)
         {
         strcpy(Keyword, ".hword");
         }
      else if (strcasecmp(Keyword, "dcd") == 0)
         {
         strcpy(Keyword, ".word");
         }
      // just a regular label - add a colon
      sprintf(pLine, "%s: %s %s %s", Label, Keyword, Opcode, Comment);
      return strlen(pLine);
      }

   return iLineLen;
} /* ParseLine() */

BOOL IsMacro(char *pLine, int iLen)
{
int i = 0;
int j;
char szWord[256];

   while (pLine[i] == ' ' && i < iLen)
      {
      i++; // skip leading spaces
      }
   j = 0;
   while (pLine[i] != ' ' && i < iLen)
      {
      szWord[j++] = pLine[i++];
      }
   szWord[j] = '\0';
   
   return (strcasecmp(szWord, "macro") == 0);
   
} /* IsMacro() */

int main(int argc, char *argv[])
{
int i, j, iSize, iLen, iLineLen;
char *p, *pOut, szLine[256], pszOut[256];

   if (argc != 2)
      {
      printf("Usage: gnu_asm <filename>\nConverts Microsoft ASM syntax to GNU (GAS)\nCopyright (c) 2012-2017 BitBank Software, Inc.\n"); 
      return 1; // no filename passed
      }
   ihandle = EMUOpenRO(argv[1]); // open input file
   if ((size_t)ihandle == -1)
      {
      printf("Unable to open file: %s\r\n", argv[1]);
      return 1; // bad filename passed
      }
   
// create output filename from input name
   iLen = strlen(argv[1]);
   for (i=iLen-1; i>=0; i--)
      if (argv[1][i] == '.') // look for file extension
         break;
   strcpy(pszOut, argv[1]);
   if (i == 0) // didn't find it
      strcat(pszOut, ".s"); // add file extension
   else
      strcpy(&pszOut[i], ".s"); // replace file extension
   ohandle = EMUCreate(pszOut);
   if ((size_t)ohandle == -1)
      {
      EMUClose(ihandle);
      printf("Unable to create output file: %s\r\n", pszOut);
      return 1;
      }
      
   iSize = EMUSeek(ihandle, 0, 2); // get the file size
   EMUSeek(ihandle, 0, 0);
   p = EMUAlloc(iSize); // allocate enough to load entire file
   pOut = EMUAlloc(iSize + iSize/10); // 10% more for inserting ":" characters
   EMURead(ihandle, p, iSize); // read the entire file
   EMUClose(ihandle);
   i = j = 0;
   while (i < iSize)
      {
      iLineLen = PreParseLine(p, &i, iSize, szLine); // read a line, convert ";" to "@" and tabs to spaces
      if (IsMacro(szLine, iLineLen)) // special case where a line is just "MACRO"
         {
         iLineLen = PreParseLine(p, &i, iSize, szLine); // read the next line
         iLineLen = ParseLine(szLine, iLineLen, TRUE); // parse and convert keywords
         }
      else
         {
         iLineLen = ParseLine(szLine, iLineLen, FALSE); // parse and convert keywords
         }
      if (iLineLen)
         {
         memcpy(&pOut[j], szLine, iLineLen); // copy to output buffer
         j += iLineLen;
         pOut[j++] = '\n'; // single newline at the end of each line
         }
      }
   EMUWrite(ohandle, pOut, j); // write the data to the new file
   EMUClose(ohandle);
   printf("%s created successfully\r\n", pszOut);
   return 0; 
} /* main() */
