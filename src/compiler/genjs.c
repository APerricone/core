/*
 * Compiler JS source generation
 *
 * Copyright 2020 Antonino Perricone <antonino.perricone@yahoo.it>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * (or visit their website at https://www.gnu.org/licenses/).
 *
 */

#include "hbcomp.h"
#include "hbdate.h"
#include "hbassert.h"

static void hb_compGenJSFunction( HB_COMP_DECL, PHB_HFUNC pFunc, FILE * yyc );
static void hb_compGenJSString( HB_COMP_DECL, FILE * yyc, HB_BYTE *pStart, HB_SIZE nLen);
static void hb_compGenJSExecute( HB_COMP_DECL, FILE * yyc, HB_BYTE *pStart);

void hb_compGenJavascript( HB_COMP_DECL, PHB_FNAME pFileName )       /* generates the JS language output */
{
   char szFileName[ HB_PATH_MAX ];
   PHB_HFUNC pFunc;
   FILE * yyc;

   if( ! pFileName->szExtension )
      pFileName->szExtension = ".js";
   hb_fsFNameMerge( szFileName, pFileName );

   yyc = hb_fopen( szFileName, "wt" );
   if( ! yyc )
   {
      hb_compGenError( HB_COMP_PARAM, hb_comp_szErrors, 'E', HB_COMP_ERR_CREATE_OUTPUT, szFileName, NULL );
      return;
   }

   if( ! HB_COMP_PARAM->fQuiet )
   {
      char buffer[ 80 + HB_PATH_MAX - 1 ];
      hb_snprintf( buffer, sizeof( buffer ),
                   "Generating Javascript output to \'%s\'... ", szFileName );
      hb_compOutStd( HB_COMP_PARAM, buffer );
   }

   pFunc = HB_COMP_PARAM->functions.pFirst;
   while( pFunc )
   {
      if( ( pFunc->funFlags & HB_FUNF_FILE_DECL ) == 0 )
      {
         hb_compGenJSFunction( HB_COMP_PARAM, pFunc, yyc );
      }
      pFunc = pFunc->pNext;
   }


   fclose( yyc );

   if( ! HB_COMP_PARAM->fQuiet )
      hb_compOutStd( HB_COMP_PARAM, "Done.\n" );
}

static void hb_compGenJSFunction( HB_COMP_DECL, PHB_HFUNC pFunc, FILE * yyc )
{
   HB_BYTE *pCode;
   HB_SIZE nPCodePos;
   HB_SIZE nComputedLen;

   /* Is it _STATICS$ - static initialization function */
   if( pFunc == HB_COMP_PARAM->pInitFunc )
      fputs( "function InitStatics()", yyc );
   /* Is it an (_INITLINES) function */
   else if( pFunc == HB_COMP_PARAM->pLineFunc )
      fputs( "function InitLines()", yyc );
   /* Is it an INIT FUNCTION/PROCEDURE */
   else if( pFunc->cScope & HB_FS_INIT )
      fprintf( yyc, "function Init_%s()", pFunc->szName );
   /* Is it an EXIT FUNCTION/PROCEDURE */
   else if( pFunc->cScope & HB_FS_EXIT )
      fprintf( yyc, "function Exit_%s()", pFunc->szName );
   /* Is it a STATIC FUNCTION/PROCEDURE */
   else if( pFunc->cScope & HB_FS_STATIC )
      fprintf( yyc, "function %s()", pFunc->szName );
   else /* Then it must be PUBLIC FUNCTION/PROCEDURE */
      fprintf( yyc, "function %s()", pFunc->szName );

   fputs(" {\nvar returnVal,stack = [];\n", yyc);

   pCode = pFunc->pCode;
   nPCodePos = 0;
   while( nPCodePos < pFunc->nPCodePos )
   {
      nComputedLen = 0;
      switch(*pCode) {
         case HB_P_ENDPROC:   /*   7 instructs the virtual machine to end execution */
            fputs("return returnVal;", yyc);
            break;
         case HB_P_DOSHORT:   /*  20 instructs the virtual machine to execute a function discarding its result */
            hb_compGenJSExecute(HB_COMP_PARAM, yyc, pCode);
            break;
         case HB_P_LINE:      /*  36 currently compiled source code line number */
            fprintf(yyc, "// line %i", HB_PCODE_MKUSHORT( pCode + 1 ));
            break;
         case HB_P_PUSHSTRSHORT: /* 106 places a string on the virtual machine stack */
            nComputedLen = 2 + *( pCode + 1);
            fputs("stack.push(", yyc);
            hb_compGenJSString( HB_COMP_PARAM, yyc, pCode + 2, nComputedLen - 3);
            fputs(");", yyc);
            break;
         case HB_P_PUSHFUNCSYM: /* 176 places a symbol on the virtual machine stack */
            fprintf(yyc, "stack.push(%s);", hb_compSymbolName( HB_COMP_PARAM, HB_PCODE_MKUSHORT( pCode + 1 ) ));
            break;
         default:
            fprintf(yyc, "// code %i TODO", *pCode);
         break;
      }
      /* Displaying as decimal is more compact than hex */
      //fprintf( yyc, "%d", ( int ) pFunc->pCode[ nPCodePos++ ] );
      if(hb_comp_pcode_len[*pCode]==0) {
         if(nComputedLen>0) {
            fputc('\n', yyc);
            nPCodePos+=nComputedLen;
            pCode+=nComputedLen;
         } else {
            fputs(" need Management!\n", yyc);
            nPCodePos+=1;
            pCode+=1;
         }
      } else {
         fputc('\n', yyc);
         nPCodePos+=hb_comp_pcode_len[*pCode];
         pCode+=hb_comp_pcode_len[*pCode];
      }
   }

   fputs("}\n\n", yyc);
}

static void hb_compGenJSString( HB_COMP_DECL, FILE * yyc, HB_BYTE *pText, HB_SIZE nLen)
{
   HB_SIZE nPos;
   HB_BYTE *uchr = pText;

   fputc( '"', yyc );
   for( nPos = 0; nPos < nLen; ++nPos, ++uchr )
   {
      if( (*uchr) == '"' )
         fputs("\\\"", yyc);
      else if( (*uchr) < ( HB_BYTE ) ' ' || (*uchr) >= 127 )
         fprintf( yyc, "\\x%02X", *uchr);
      else
         fputc( *uchr, yyc );
   }
   fputc( '"', yyc );
}

static void hb_compGenJSExecute( HB_COMP_DECL, FILE * yyc, HB_BYTE *pStart)
{
   HB_SIZE nParam = *(pStart+1);
   fprintf( yyc,"{\nvar tmp = stack.splice(stack.length-%i);\nstack.pop().apply(undefined,tmp);\n}", *(pStart+1));
}