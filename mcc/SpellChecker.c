/***************************************************************************

 TextEditor.mcc - Textediting MUI Custom Class
 Copyright (C) 1997-2000 Allan Odgaard
 Copyright (C) 2005 by TextEditor.mcc Open Source Team

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 TextEditor class Support Site:  http://www.sf.net/projects/texteditor-mcc

 $Id$

***************************************************************************/

#include <stdio.h>
#include <string.h>

#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/locale.h>
#include <proto/muimaster.h>
#include <proto/rexxsyslib.h>
#include <rexx/storage.h>

#include "TextEditor_mcc.h"
#include "private.h"

#include "SDI_hook.h"

HOOKPROTONH(SelectCode, void, void *lvobj, long **parms)
{
    struct InstData *data = (struct InstData *)*parms;
    struct marking block;
    char *entry;

  DoMethod(lvobj, MUIM_List_GetEntry, MUIV_List_GetEntry_Active, &entry);
  if(entry)
  {
      int length = strlen(entry);
      UWORD oldpos;

    if(Enabled(data))
      Key_Clear(data);

    block.startline = data->actualline;
    block.startx    = data->CPos_X;
    block.stopline    = data->actualline;
    block.stopx     = data->CPos_X+length;
    AddToUndoBuffer(pasteblock, (char *)&block, data);
    oldpos = data->CPos_X;
    data->CPos_X += length;
    PasteChars(oldpos, data->actualline, length, entry, NULL, data);
  }
  set(data->SuggestWindow, MUIA_Window_Open, FALSE);
  DoMethod(lvobj, MUIM_List_Clear);
  set(data->object, MUIA_TextEditor_AreaMarked, FALSE);
}
MakeStaticHook(SelectHook, SelectCode);

long SendRexx (char *word, char *command, struct InstData *data)
{
    struct MsgPort *rexxport;
    struct RexxMsg *rxmsg;
    char    buffer[512];
    LONG    result = FALSE;

  if((rexxport = FindPort("REXX")) && (data->clipport = CreateMsgPort()))
  {
    rxmsg = CreateRexxMsg(data->clipport, NULL, NULL);
    rxmsg->rm_Action = RXCOMM;
    sprintf(buffer, command, word);
    rxmsg->rm_Args[0] = (UBYTE *)CreateArgstring(buffer, strlen(buffer));

    PutMsg(rexxport, (struct Message *)rxmsg);
    if(Wait((1 << data->clipport->mp_SigBit) | SIGBREAKF_CTRL_C) != SIGBREAKF_CTRL_C)
    {
      GetMsg(data->clipport);

      if(rxmsg->rm_Result1 == 0)
      {
        result = TRUE;
        DeleteArgstring((APTR)rxmsg->rm_Result2);
      }
    }
    DeleteArgstring((APTR)rxmsg->rm_Args[0]);
    DeleteRexxMsg(rxmsg);
    DeleteMsgPort(data->clipport);
  }
  return result;
}

//void *CloneWBPath(struct WBStartup *, struct  Library *);
//void FreeWBPath(void *, struct  Library *);

long SendCLI(char *word, char *command, UNUSED struct InstData *data)
{
  char buffer[512];
  long result = TRUE;
  BPTR path;

  sprintf(buffer, command, word);

  #warning "CloneWB/FreeWB necessary?"
  //path = CloneWBPath(NULL, DOSBase);
  if(SystemTags(buffer, /*NP_Path, path,*/ TAG_DONE) == -1)
  {
    result = FALSE;
    //FreeWBPath(path, DOSBase);
  }
  return(result);
}

void *SuggestWindow (struct InstData *data)
{
    void *lvobj;

  data->SuggestWindow = WindowObject,
        MUIA_Window_Borderless,     TRUE,
        MUIA_Window_CloseGadget,    FALSE,
        MUIA_Window_DepthGadget,    FALSE,
        MUIA_Window_DragBar,        FALSE,
        MUIA_Window_SizeGadget,     FALSE,
        WindowContents, VGroup,
          MUIA_InnerBottom,       0,
          MUIA_InnerLeft,         0,
          MUIA_InnerRight,        0,
          MUIA_InnerTop,          0,
          MUIA_Group_PageMode,      TRUE,
          Child, ListviewObject,
            MUIA_Listview_Input, FALSE,
            MUIA_Listview_List, FloattextObject,
              MUIA_Floattext_Text,  "Word is spelled correctly.",
              MUIA_Frame, MUIV_Frame_ReadList,
              MUIA_Background, MUII_ListBack,
              End,
            End,
          Child, lvobj = ListviewObject,
            MUIA_Listview_List, ListObject,
              MUIA_Frame, MUIV_Frame_InputList,
              MUIA_Background, MUII_ListBack,
              MUIA_List_ConstructHook, MUIV_List_ConstructHook_String,
              MUIA_List_DestructHook, MUIV_List_DestructHook_String,
              MUIA_List_Pool, data->mypool,
              End,
            End,
          End,
        End;

  DoMethod(lvobj, MUIM_Notify, MUIA_Listview_DoubleClick, TRUE,
      MUIV_Notify_Self, 3, MUIM_CallHook, &SelectHook, data);

  DoMethod(data->SuggestWindow, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
      MUIV_Notify_Self, 3, MUIM_Set, MUIA_Window_Open, FALSE);

  DoMethod(data->SuggestWindow, MUIM_Notify, MUIA_Window_Open, MUIV_EveryTime,
      data->object, 3, MUIM_Set, MUIA_TextEditor_PopWindow_Open, MUIV_TriggerValue);

  data->SuggestListview = lvobj;
  return(data->SuggestWindow);
}

BOOL LookupWord(STRPTR word, struct InstData *data)
{
    ULONG res;

  if(data->LookupSpawn)
      res = SendRexx(word, data->LookupCmd, data);
  else  res = SendCLI(word, data->LookupCmd, data);
  if(res)
  {
    GetVar("Found", (STRPTR)&res, 4, GVF_GLOBAL_ONLY);
    if(res >> 24 == '0')
    {
      return(FALSE);
    }
  }
  return(TRUE);
}

void SuggestWord (struct InstData *data)
{
    ULONG   top, left;
    LONG    line_nr;
    struct  pos_info pos;
    struct  line_node *line = data->actualline;

  if(IsAlpha(data->mylocale, *(line->line.Contents+data->CPos_X)))
  {
    if(Enabled(data))
    {
      data->blockinfo.enabled = FALSE;
      MarkText(data->blockinfo.startx, data->blockinfo.startline, data->blockinfo.stopx, data->blockinfo.stopline, data);
    }
/*    else
    {
      SetCursor(data->CPos_X, line, FALSE, data);
    }
*/  

    while(data->CPos_X && (IsAlpha(data->mylocale, *(line->line.Contents+data->CPos_X-1)) || *(line->line.Contents+data->CPos_X-1) == '-' || (*(line->line.Contents+data->CPos_X-1) == '\'')))
    {
      GoPreviousWord(data);
    }

    line = data->actualline;
    data->blockinfo.startx = data->CPos_X;
    data->blockinfo.startline = line;

    line_nr = LineToVisual(line, data) - 1;
    OffsetToLines(data->CPos_X, line, &pos, data);
    get(_win(data->object), MUIA_Window_LeftEdge, &left);
    get(_win(data->object), MUIA_Window_TopEdge, &top);
    left  += data->xpos + FlowSpace(line->line.Flow, line->line.Contents+(data->CPos_X-pos.x), data) + MyTextLength(data->font, line->line.Contents+(data->CPos_X-pos.x), pos.x);
    top += data->ypos + (data->height * (line_nr + pos.lines));

    while(data->CPos_X < line->line.Length && (IsAlpha(data->mylocale, *(line->line.Contents+data->CPos_X)) || *(line->line.Contents+data->CPos_X) == '-' || *(line->line.Contents+data->CPos_X) == '\''))
    {
      data->CPos_X++;
    }
    data->blockinfo.stopx = data->CPos_X;
    data->blockinfo.stopline = line;

    data->blockinfo.enabled = TRUE;
    MarkText(data->blockinfo.startx, data->blockinfo.startline, data->blockinfo.stopx, data->blockinfo.stopline, data);

    SetAttrs(data->SuggestWindow,
                MUIA_Window_Open,     FALSE,
                MUIA_Window_LeftEdge, left,
                MUIA_Window_TopEdge,    top,
                TAG_DONE);

    if(data->blockinfo.stopx-data->blockinfo.startx < 256)
    {
        char word[256];
        long res;

      strncpy(word, line->line.Contents+data->blockinfo.startx, data->blockinfo.stopx-data->blockinfo.startx);
      word[data->blockinfo.stopx-data->blockinfo.startx] = '\0';

      set(_win(data->object), MUIA_Window_Sleep, TRUE);

      if((data->flags & FLG_CheckWords) && LookupWord(word, data))
      {
        Object *group;

        get(data->SuggestWindow, MUIA_Window_RootObject, (APTR)&group);
        set(group, MUIA_Group_ActivePage, MUIV_Group_ActivePage_First);
        SetAttrs(data->SuggestWindow,
              MUIA_Window_Activate, TRUE,
              MUIA_Window_DefaultObject, NULL,
              MUIA_Window_Open, TRUE,
              TAG_DONE);
      }
      else
      {
        if(data->SuggestSpawn)
            res = SendRexx(word, data->SuggestCmd, data);
        else  res = SendCLI(word, data->SuggestCmd, data);
        if(res)
        {
            BPTR  fh;

          if((fh = Open("T:Matches", MODE_OLDFILE)))
          {
              char    entry[128];
              Object  *group;

            DoMethod(data->SuggestListview, MUIM_List_Clear);
            while(FGets(fh, entry, 128))
            {
              entry[strlen(entry)-1] = '\0';
              DoMethod(data->SuggestListview, MUIM_List_InsertSingle, entry, MUIV_List_Insert_Sorted);
            }
            Close(fh);

            get(data->SuggestWindow, MUIA_Window_RootObject, (APTR)&group);
            set(group, MUIA_Group_ActivePage, MUIV_Group_ActivePage_Last);
            SetAttrs(data->SuggestWindow,
                  MUIA_Window_Activate, TRUE,
                  MUIA_Window_DefaultObject, data->SuggestListview,
                  MUIA_Window_Open, TRUE,
                  TAG_DONE);
          }
        }
      }
      set(_win(data->object), MUIA_Window_Sleep, FALSE);
    }
  }
  else
  {
    DisplayBeep(NULL);
  }
}

void CheckWord (struct InstData *data)
{
  if(data->TypeAndSpell && data->CPos_X && IsAlpha(data->mylocale, *(data->actualline->line.Contents+data->CPos_X-1)))
  {
      char word[256];
      LONG  start, end = data->CPos_X;
      struct line_node *line = data->actualline;

    do {

      GoPreviousWord(data);

    } while(data->CPos_X && data->actualline == line && (*(data->actualline->line.Contents+data->CPos_X-1) == '-' || *(data->actualline->line.Contents+data->CPos_X-1) == '\''));

    start = data->CPos_X;
    data->CPos_X = end;

    if(start-end < 256 && data->actualline == line)
    {
      strncpy(word, data->actualline->line.Contents+start, end-start);
      word[end-start] = '\0';
      if(!LookupWord(word, data))
        DisplayBeep(NULL);
    }
    else
    {
      data->actualline = line;
    }
  }
}
