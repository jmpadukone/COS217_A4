#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "dynarray.h"
#include "nodeFT.h"
#include <stdio.h>

/* A node in a FT */
struct node
{

   /* the boolean value indicating if the variable is a directory or file */
   boolean isDir;
   /* the object corresponding to the node's absolute path */
   Path_T oPPath;
   /* this node's parent */
   Node_T oNParent;
   /* the object containing links to this node's directory children (if directory) */
   DynArray_T oDDirChildren;
   /* the object containing links to this node's file children (if directory) */
   DynArray_T oDFileChildren;
   /* the pointer to this node's contents (if file) */
   void* fileContent;
   /* length of file contents */
   size_t fileSize;
};


/*
  Links new child oNChild into oNParent's children array at index
  ulIndex. Returns SUCCESS if the new child was added successfully,
  or  MEMORY_ERROR if allocation fails adding oNChild to the array.
*/
static int Node_addChild(Node_T oNParent, Node_T oNChild, size_t ulIndex, boolean isDirec)
{
   
   assert(oNParent != NULL);
   assert(oNChild != NULL);

   
   if(isDirec) {
      if(DynArray_addAt(oNParent->oDDirChildren, ulIndex, oNChild))
         return SUCCESS;
      else
         return MEMORY_ERROR;
   }
   else {
      if(DynArray_addAt(oNParent->oDFileChildren, ulIndex, oNChild))
         return SUCCESS;
      else
         return MEMORY_ERROR;
   }
}

/*
  Compares the string representation of oNfirst with a string
  pcSecond representing a node's path.
  Returns <0, 0, or >0 if oNFirst is "less than", "equal to", or
  "greater than" pcSecond, respectively.
*/
static int Node_compareString(const Node_T oNFirst, const char *pcSecond)
{
   assert(oNFirst != NULL);
   assert(pcSecond != NULL);


   return Path_compareString(oNFirst->oPPath, pcSecond);
}


/*
  Creates a new node with path oPPath and parent oNParent.  Returns an
  int SUCCESS status and sets *poNResult to be the new node if
  successful. Otherwise, sets *poNResult to NULL and returns status:
  * MEMORY_ERROR if memory could not be allocated to complete request
  * CONFLICTING_PATH if oNParent's path is not an ancestor of oPPath
  * NO_SUCH_PATH if oPPath is of depth 0
                 or oNParent's path is not oPPath's direct parent
                 or oNParent is NULL but oPPath is not of depth 1
  * ALREADY_IN_TREE if oNParent already has a child with this path
*/
int Node_new(boolean isDirec, Path_T oPPath, Node_T oNParent, Node_T *poNResult, void *pvContents, size_t ulLength)
{
   
   struct node *psNew;
   Path_T oPParentPath = NULL;
   Path_T oPNewPath = NULL;
   size_t ulParentDepth;
   size_t ulIndex;
   int iStatus;
   boolean isDir;
 
   assert(oPPath != NULL);


   /* allocate space for a new node */
   psNew = malloc(sizeof(struct node));
   if(psNew == NULL) {
      *poNResult = NULL;
      return MEMORY_ERROR;
   }

   /* set the new node's path */
   iStatus = Path_dup(oPPath, &oPNewPath);
   if(iStatus != SUCCESS) {
      free(psNew);
      *poNResult = NULL;
      return iStatus;
   }
   psNew->oPPath = oPNewPath;

  
   /* validate and set the new node's parent */
   if(oNParent != NULL) {
      size_t ulSharedDepth;

      oPParentPath = oNParent->oPPath;
      ulParentDepth = Path_getDepth(oPParentPath);
      ulSharedDepth = Path_getSharedPrefixDepth(psNew->oPPath,
                                                oPParentPath);
      /* parent must be an ancestor of child */
      if(ulSharedDepth < ulParentDepth) {
         Path_free(psNew->oPPath);
         free(psNew);
         *poNResult = NULL;
         return CONFLICTING_PATH;
      }

      /* parent must be exactly one level up from child */
      if(Path_getDepth(psNew->oPPath) != ulParentDepth + 1) {
         Path_free(psNew->oPPath);
         free(psNew);
         *poNResult = NULL;
         return NO_SUCH_PATH;
      }

      /* parent must not already have child with this path */
      if(Node_hasChild(oNParent, oPPath, &ulIndex, &isDir)) {
         Path_free(psNew->oPPath);
         free(psNew);
         *poNResult = NULL;
         return ALREADY_IN_TREE;
      }
   }
   else {
      /* new node must be root */
      /* can only create one "level" at a time */
      if(Path_getDepth(psNew->oPPath) != 1) {
         Path_free(psNew->oPPath);
         free(psNew);
         *poNResult = NULL;
         return NO_SUCH_PATH;
      }
   }
   psNew->oNParent = oNParent;

   /* initialize the new node */
      psNew->oDDirChildren = DynArray_new(0);
      psNew->oDFileChildren = DynArray_new(0);
      if(psNew->oDDirChildren == NULL || psNew->oDFileChildren == NULL) {
         Path_free(psNew->oPPath);
         free(psNew);
         *poNResult = NULL;
         return MEMORY_ERROR;
      }
      psNew->isDir = isDirec;
      
   /* Link into parent's children list */
   if(oNParent != NULL) {
      iStatus = Node_addChild(oNParent, psNew, ulIndex, psNew->isDir);
      if(iStatus != SUCCESS) {
         Path_free(psNew->oPPath);
         free(psNew);
         *poNResult = NULL;
         return iStatus;
      }
   }
   

   if(!(isDirec)) {
      psNew->fileContent = pvContents;
      psNew->fileSize = ulLength;
   }
   else {
      psNew->fileContent = NULL;
      psNew->fileSize = 0;
   }
   psNew->isDir = isDirec;

   *poNResult = psNew;
   
   return SUCCESS;
}

size_t Node_free(Node_T oNNode)
{
   
   size_t ulIndex;
   size_t ulCount = 0;
   
   assert(oNNode != NULL);
   
   /* remove from parent's list */
   if(oNNode->oNParent != NULL) {
      if(oNNode->oNParent->oDDirChildren != NULL &&
         DynArray_bsearch(
            oNNode->oNParent->oDDirChildren,
            oNNode, &ulIndex,
            (int (*)(const void *, const void *)) Node_compare)
         )
         (void) DynArray_removeAt(oNNode->oNParent->oDDirChildren,
                                  ulIndex);
      
      if(oNNode->oNParent->oDFileChildren != NULL &&
         DynArray_bsearch(
            oNNode->oNParent->oDFileChildren,
            oNNode, &ulIndex,
            (int (*)(const void *, const void *)) Node_compare))
         (void) DynArray_removeAt(oNNode->oNParent->oDFileChildren,
                                  ulIndex);
   }


         /* recursively remove children */
         while(DynArray_getLength(oNNode->oDDirChildren) != 0) {
            ulCount += Node_free(DynArray_get(oNNode->oDDirChildren, 0));
         }
         
         DynArray_free(oNNode->oDDirChildren);
         oNNode->oDDirChildren = NULL;
         
         while(DynArray_getLength(oNNode->oDFileChildren) != 0){ 
            
            ulCount += Node_free(DynArray_get(oNNode->oDFileChildren, 0));
         }
            
         DynArray_free(oNNode->oDFileChildren);
         oNNode->oDFileChildren = NULL;
 

    
      /* remove path */
      Path_free(oNNode->oPPath);

      /* finally, free the struct node */
      free(oNNode);
      ulCount++;
      return ulCount;
 
}

size_t Node_destroyFree(Node_T oNNode) {

   size_t ulCount;
   Node_T child;
   size_t i;
   
   assert(oNNode != NULL);

   for(i=0;i < DynArray_getLength(oNNode->oDDirChildren); i++) {
  
      (void) Node_getChild(TRUE, oNNode, i, &child);
      ulCount += Node_free(child);
   }

   DynArray_free(oNNode->oDDirChildren);

   for(i=0;i < DynArray_getLength(oNNode->oDFileChildren); i++) {
 
      (void) Node_getChild(FALSE, oNNode, i, &child);
      free(child);
      ulCount++;
   }

   DynArray_free(oNNode->oDFileChildren);

   free(oNNode);
   ulCount++;

   return ulCount;
}

Path_T Node_getPath(Node_T oNNode)
{
   assert(oNNode != NULL);

   return oNNode->oPPath;
}

boolean Node_hasChild(Node_T oNParent, Path_T oPPath,
                      size_t *pulChildID, boolean *isDir) {
   int i;
   int j;
   
   assert(oNParent != NULL);
   assert(oPPath != NULL);
   assert(pulChildID != NULL);

   i =  DynArray_bsearch(oNParent->oDDirChildren, (char*) Path_getPathname(oPPath), pulChildID,
                      (int (*)(const void*,const void*)) Node_compareString);

   if(i == 1) {
      *isDir = TRUE;
      return i;
   }
   
   j =  DynArray_bsearch(oNParent->oDFileChildren, (char*) Path_getPathname(oPPath), pulChildID,
                         (int (*)(const void*,const void*)) Node_compareString);
    
      if(j == 1) {
      *isDir = FALSE;
      return j;
   }
   else
      return 0;
}



size_t Node_getNumDirChildren(Node_T oNParent)
{
   assert(oNParent != NULL);
   return DynArray_getLength(oNParent->oDDirChildren);
}

size_t Node_getNumFileChildren(Node_T oNParent)
{
   assert(oNParent != NULL);
   return DynArray_getLength(oNParent->oDFileChildren);
}

size_t Node_getNumChildren(Node_T oNParent)
{
   assert(oNParent != NULL);
   return Node_getNumDirChildren(oNParent) + Node_getNumFileChildren(oNParent);
}

int  Node_getChild(boolean childIsDir, Node_T oNParent, size_t ulChildID, Node_T *poNResult)
{   
   assert(oNParent != NULL);
   assert(poNResult != NULL);

   /* ulChildID is the index into oNParent->oDDirChildren or oNParent->oDFileChildren*/
      if (childIsDir) {
         if (ulChildID >= Node_getNumDirChildren(oNParent)) {
            *poNResult = NULL;
            return NO_SUCH_PATH;
         }
         else {
            *poNResult = DynArray_get(oNParent->oDDirChildren, ulChildID);
            return SUCCESS;
         }
      }
      else {
         if (ulChildID >= Node_getNumFileChildren(oNParent)) {
            *poNResult = NULL;
            return NO_SUCH_PATH;
         }
         else {
            *poNResult = DynArray_get(oNParent->oDFileChildren, ulChildID);
            return SUCCESS;
         }
      }
}

Node_T Node_getParent(Node_T oNNode)
{
   assert(oNNode != NULL);

   return oNNode->oNParent;
}

int Node_compare(Node_T oNFirst, Node_T oNSecond)
{
   assert(oNFirst != NULL);
   assert(oNSecond != NULL);

   if(oNFirst->isDir == FALSE && oNFirst->isDir == TRUE)
      return -1;
   if(oNFirst->isDir == TRUE && oNFirst->isDir == FALSE)
      return 1;
      
   return Path_comparePath(oNFirst->oPPath, oNSecond->oPPath);
}

void *Node_getFileContents(Node_T oNNode)
{

   assert(oNNode != NULL);

   return oNNode->fileContent;
}

size_t Node_getFileSize(Node_T oNNode)
{

   assert(oNNode != NULL);

   return oNNode->fileSize;
}

void *Node_replaceFileContents(Node_T oNNode, void *pvNewContents, size_t ulNewLength)
{
   void *oldContents;
   
   assert(oNNode != NULL);

   /*  oldContents = oNNode->fileContent;
   contents=realloc(oNNode->fileContent, ulNewLength);
   contents = pvNewContents;
   oNNode->fileContent = contents;
   oNNode->fileSize = ulNewLength; */
   oldContents = oNNode->fileContent;
   oNNode->fileContent = pvNewContents;
   oNNode->fileSize = ulNewLength;
   return oldContents;
}

boolean Node_isDir(Node_T oNNode)
{

   assert(oNNode != NULL);
   return oNNode->isDir;
}

void Node_setDir(Node_T oNNode, boolean isDirec)
{

   assert(oNNode != NULL);
   oNNode->isDir = isDirec;
}

char *Node_toString(Node_T oNNode)
{
   char *copyPath;

   assert(oNNode != NULL);

   copyPath = malloc(Path_getStrLength(Node_getPath(oNNode))+1);
   if(copyPath == NULL)
      return NULL;
   else
      return strcpy(copyPath, Path_getPathname(Node_getPath(oNNode)));
}
