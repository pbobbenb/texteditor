/***************************************************************************

 TextEditor.mcc - Textediting MUI Custom Class
 Copyright (C) 1997-2000 Allan Odgaard
 Copyright (C) 2005-2021 TextEditor.mcc Open Source Team

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

#include <proto/exec.h>

#include "SDI_compiler.h"

#include "Debug.h"

// GetSucc()
// get a node's sucessor

struct Node *GetSucc(struct Node *node)
{
  struct Node *result = NULL;

  ENTER();

  if(node != NULL && node->ln_Succ != NULL && node->ln_Succ->ln_Succ != NULL)
    result = node->ln_Succ;

  RETURN(result);
  return result;
}
