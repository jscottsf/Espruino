/*
 * This file is part of Espruino, a JavaScript interpreter for Microcontrollers
 *
 * Copyright (C) 2013 Gordon Williams <gw@pur3.co.uk>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * ----------------------------------------------------------------------------
 * Variables
 * ----------------------------------------------------------------------------
 */
#ifndef JSVAR_H_
#define JSVAR_H_

#include "jsutils.h"


typedef void (*JsCallback)(JsVarRef var) 
#ifdef SDCC
__reentrant
#endif
;

/** To avoid confusion - JsVarRefCounter should be big enough
 * to store as many refs as can possibly be created - so it's
 * safe just to set it to the same size as JsVarRef. However
 * it is NOT a reference itself.
 */
typedef JsVarRef JsVarRefCounter;

typedef enum {
  ARRAYBUFFERVIEW_UNDEFINED = 0,
  ARRAYBUFFERVIEW_ARRAYBUFFER = 1 | 64, ///< Basic ArrayBuffer type

  ARRAYBUFFERVIEW_MASK_SIZE = 15,
  ARRAYBUFFERVIEW_SIGNED = 16,
  ARRAYBUFFERVIEW_FLOAT = 32,

  ARRAYBUFFERVIEW_UINT8   = 1,
  ARRAYBUFFERVIEW_INT8    = 1 | ARRAYBUFFERVIEW_SIGNED,
  ARRAYBUFFERVIEW_UINT16  = 2,
  ARRAYBUFFERVIEW_INT16   = 2 | ARRAYBUFFERVIEW_SIGNED,
  ARRAYBUFFERVIEW_UINT32  = 4,
  ARRAYBUFFERVIEW_INT32   = 4 | ARRAYBUFFERVIEW_SIGNED,
  ARRAYBUFFERVIEW_FLOAT32 = 4 | ARRAYBUFFERVIEW_FLOAT,
  ARRAYBUFFERVIEW_FLOAT64 = 8 | ARRAYBUFFERVIEW_FLOAT,
} PACKED_FLAGS JsVarDataArrayBufferViewType;
#define JSV_ARRAYBUFFER_GET_SIZE(T) (size_t)((T)&ARRAYBUFFERVIEW_MASK_SIZE)
#define JSV_ARRAYBUFFER_IS_SIGNED(T) (((T)&ARRAYBUFFERVIEW_SIGNED)!=0)
#define JSV_ARRAYBUFFER_IS_FLOAT(T) (((T)&ARRAYBUFFERVIEW_FLOAT)!=0)

#define JSV_ARRAYBUFFER_MAX_LENGTH 65535

typedef struct {
  unsigned short byteOffset;
  unsigned short length;
  JsVarDataArrayBufferViewType type;
} PACKED_FLAGS JsVarDataArrayBufferView;

typedef union {
    char str[JSVAR_DATA_STRING_LEN]; ///< The contents of this variable if it is a string
    /* NOTE: For str above, we INTENTIONALLY OVERFLOW str (and hence data) in the case of STRING_EXTS
     * to overwrite 3 references in order to grab another 6 bytes worth of string data */
    // TODO do some magic with union/structs in order to make sure we don't intentionally write off the end of arrays
    JsVarInt integer; ///< The contents of this variable if it is an int
    JsVarFloat floating; ///< The contents of this variable if it is a double
    JsCallback callback; ///< Callback for native functions, or 0
    JsVarDataArrayBufferView arraybuffer; ///< information for array buffer views.
} PACKED_FLAGS JsVarData;

typedef struct {
#ifdef LARGE_MEM
  JsVarRef this; ///< The reference of this variable itself (so we can get back)
#endif
  JsVarFlags flags; ///< the flags determine the type of the variable - int/double/string/etc.

  JsVarData varData;
  /* NOTE: WE INTENTIONALLY OVERFLOW data in the case of STRING_EXTS
   * to overwrite the following 3 references in order to grab another
   * 6 bytes worth of string data */

  /* For Variable NAMES (e.g. Object/Array keys) these store actual next/previous pointers for a linked list
   * For STRING_EXT - extra characters
   * Not used for other stuff
   */
  JsVarRef nextSibling;
  JsVarRef prevSibling;

  JsVarRefCounter refs; ///< The number of references held to this - used for automatic garbage collection. NOT USED for STRINGEXT though (it is just extra characters)

  /**
   * For OBJECT/ARRAY/FUNCTION - this is the first child
   * For NAMES and REF - this is a link to the variable it points to
   * For STRING_EXT - extra character data (NOT a link)
   * For ARRAYBUFFER - a link to a string containing the data for the array buffer   *
   */
  JsVarRef firstChild;

  /**
   * For OBJECT/ARRAY/FUNCTION - this is the last child
   * For STRINGS/STRING_EXT/NAME+STRING - this is a link to more string data if it is needed
   * For REF - this is the 'parent' that the firstChild is a member of
   */
  JsVarRef lastChild;
} PACKED_FLAGS JsVar;

/* We have a few different types:
 *
 *  OBJECT/ARRAY - uses firstChild/lastChild to link to NAMEs.
 *  BUILT-IN OBJECT - as above, but we use varData to store the name as well. This means built in object names must be LESS THAN 8 CHARACTERS
 *  FUNCTION - uses firstChild/lastChild to link to NAMEs, and callback is used
 *  NAME - use nextSibling/prevSibling linking to other NAMEs, and firstChild to link to a Variable of some kind
 *  STRING - use firstChild to link to other STRINGs if String value is too long
 *  INT/DOUBLE - firstChild never used
 */

static inline unsigned char jsvGetLocks(JsVar *v) { return (unsigned char)((v->flags>>JSV_LOCK_SHIFT) & JSV_LOCK_MAX); }

// For debugging/testing ONLY - maximum # of vars we are allowed to use
void jsvSetMaxVarsUsed(unsigned int size);

// Init/kill vars as a whole
void jsvInit();
void jsvKill();
void jsvSoftInit(); ///< called when loading from flash
void jsvSoftKill(); ///< called when saving to flash
JsVar *jsvFindOrCreateRoot(); ///< Find or create the ROOT variable item - used mainly if recovering from a saved state.
unsigned int jsvGetMemoryUsage(); ///< Get number of memory records (JsVars) used
unsigned int jsvGetMemoryTotal(); ///< Get total amount of memory records
bool jsvIsMemoryFull(); ///< Get whether memory is full or not
void jsvShowAllocated(); ///< Show what is still allocated, for debugging memory problems
/// Try and allocate more memory - only works if RESIZABLE_JSVARS is defined
void jsvSetMemoryTotal(unsigned int jsNewVarCount);


// Note that jsvNew* don't REF a variable for you, but the do LOCK it
JsVar *jsvNew(); ///< Create a new variable
JsVar *jsvNewWithFlags(JsVarFlags flags);
JsVar *jsvNewFromString(const char *str); ///< Create a new string
JsVar *jsvNewStringOfLength(unsigned int byteLength); ///< Create a new string of the given length - full of 0s
static inline JsVar *jsvNewFromEmptyString() { return jsvNewWithFlags(JSV_STRING); } ;///< Create a new empty string
static inline JsVar *jsvNewNull() { return jsvNewWithFlags(JSV_NULL); } ;///< Create a new null variable
/** Create a new variable from a substring. argument must be a string. stridx = start char or str, maxLength = max number of characters (can be JSVAPPENDSTRINGVAR_MAXLENGTH)  */
JsVar *jsvNewFromStringVar(const JsVar *str, size_t stridx, size_t maxLength);
JsVar *jsvNewFromInteger(JsVarInt value);
JsVar *jsvNewFromBool(bool value);
JsVar *jsvNewFromFloat(JsVarFloat value);
// Turns var into a Variable name that links to the given value... No locking so no need to unlock var
JsVar *jsvMakeIntoVariableName(JsVar *var, JsVar *valueOrZero);
JsVar *jsvNewFromPin(int pin);

/// DO NOT CALL THIS DIRECTLY - this frees an unreffed/locked var
void jsvFreePtr(JsVar *var);

/// Get a reference from a var - SAFE for null vars
JsVarRef jsvGetRef(JsVar *var);

/// SCARY - only to be used for vital stuff like load/save
JsVar *_jsvGetAddressOf(JsVarRef ref);

/// Lock this reference and return a pointer - UNSAFE for null refs
JsVar *jsvLock(JsVarRef ref);

/// Lock this pointer and return a pointer - UNSAFE for null pointer
JsVar *jsvLockAgain(JsVar *var);

/// Unlock this variable - this is SAFE for null variables
void jsvUnLock(JsVar *var);


/// Reference - set this variable as used by something
JsVar *jsvRef(JsVar *v);

/// Unreference - set this variable as not used by anything
void jsvUnRef(JsVar *var);

/// Helper fn, Reference - set this variable as used by something
JsVarRef jsvRefRef(JsVarRef ref);

/// Helper fn, Unreference - set this variable as not used by anything
JsVarRef jsvUnRefRef(JsVarRef ref);

static inline bool jsvIsRoot(const JsVar *v) { return v && (v->flags&JSV_VARTYPEMASK)==JSV_ROOT; }
static inline bool jsvIsPin(const JsVar *v) { return v && (v->flags&JSV_VARTYPEMASK)==JSV_PIN; }
static inline bool jsvIsInt(const JsVar *v) { return v && ((v->flags&JSV_VARTYPEMASK)==JSV_INTEGER || (v->flags&JSV_VARTYPEMASK)==JSV_PIN); }
static inline bool jsvIsFloat(const JsVar *v) { return v && (v->flags&JSV_VARTYPEMASK)==JSV_FLOAT; }
static inline bool jsvIsBoolean(const JsVar *v) { return v && (v->flags&JSV_VARTYPEMASK)==JSV_BOOLEAN; }
static inline bool jsvIsString(const JsVar *v) { return v && (v->flags&JSV_VARTYPEMASK)>=JSV_STRING_0 && (v->flags&JSV_VARTYPEMASK)<=JSV_STRING_MAX; }
static inline bool jsvIsStringExt(const JsVar *v) { return v && (v->flags&JSV_VARTYPEMASK)>=JSV_STRING_EXT_0 && (v->flags&JSV_VARTYPEMASK)<=JSV_STRING_EXT_MAX; } ///< The extra bits dumped onto the end of a string to store more data
static inline bool jsvIsNumeric(const JsVar *v) { return v && (v->flags&JSV_VARTYPEMASK)>=JSV_NUMERICSTART && (v->flags&JSV_VARTYPEMASK)<=JSV_NUMERICEND; }
static inline bool jsvIsFunction(const JsVar *v) { return v && (v->flags&JSV_VARTYPEMASK)==JSV_FUNCTION; }
static inline bool jsvIsFunctionParameter(const JsVar *v) { return v && (v->flags&JSV_FUNCTION_PARAMETER) == JSV_FUNCTION_PARAMETER; }
static inline bool jsvIsObject(const JsVar *v) { return v && (((v->flags&JSV_VARTYPEMASK)==JSV_OBJECT) || ((v->flags&JSV_VARTYPEMASK)==JSV_ROOT)); }
static inline bool jsvIsArray(const JsVar *v) { return v && (v->flags&JSV_VARTYPEMASK)==JSV_ARRAY; }
static inline bool jsvIsArrayBuffer(const JsVar *v) { return v && (v->flags&JSV_VARTYPEMASK)==JSV_ARRAYBUFFER; }
static inline bool jsvIsArrayBufferName(const JsVar *v) { return v && (v->flags&(JSV_VARTYPEMASK|JSV_NAME))==JSV_ARRAYBUFFERNAME; }
static inline bool jsvIsNative(const JsVar *v) { return v && (v->flags&JSV_NATIVE)!=0; }
static inline bool jsvIsUndefined(const JsVar *v) { return v==0; }
static inline bool jsvIsNull(const JsVar *v) { return v && (v->flags&JSV_VARTYPEMASK)==JSV_NULL; }
static inline bool jsvIsBasic(const JsVar *v) { return jsvIsNumeric(v) || jsvIsString(v);} ///< Is this *not* an array/object/etc
static inline bool jsvIsName(const JsVar *v) { return v && (v->flags & JSV_NAME)!=0; } ///< NAMEs are what's used to name a variable (it is not the data itself)

/// Can the given variable be converted into an integer without loss of precision
static inline bool jsvIsIntegerish(const JsVar *v) { return jsvIsInt(v) || jsvIsPin(v) || jsvIsBoolean(v) || jsvIsNull(v); }

static inline bool jsvIsIterable(const JsVar *v) {
  return jsvIsArray(v) || jsvIsObject(v) || jsvIsFunction(v) ||
         jsvIsString(v) || jsvIsArrayBuffer(v);
}

/** Does this string contain only Numeric characters (with optional '-' at the front)? NOT '.'/'e' and similar (allowDecimalPoint is for '.' only) */
bool jsvIsStringNumericInt(const JsVar *var, bool allowDecimalPoint);
/** Does this string contain only Numeric characters? This is for arrays
 * and makes the assertion that int_to_string(string_to_int(var))==var */
bool jsvIsStringNumericStrict(const JsVar *var);

// TODO: maybe isName shouldn't include ArrayBufferName?
bool jsvHasCharacterData(const JsVar *v); ///< does the v->data union contain character data?
bool jsvHasStringExt(const JsVar *v);
/// Does this variable use firstChild/lastChild to point to multiple childrem
bool jsvHasChildren(const JsVar *v);
/// Is this variable a type that uses firstChild to point to a single Variable (ie. it doesn't have multiple children)
bool jsvHasSingleChild(const JsVar *v);
/// Does this variable have a 'ref' argument? Stringexts use it for extra character data
static inline bool jsvHasRef(const JsVar *v) { return !jsvIsStringExt(v); }

/// This is the number of characters a JsVar can contain, NOT string length
static inline size_t jsvGetMaxCharactersInVar(const JsVar *v) {
  // see jsvCopy - we need to know about this in there too
  if (jsvIsStringExt(v)) return JSVAR_DATA_STRING_MAX_LEN;
  assert(jsvHasCharacterData(v));
  return JSVAR_DATA_STRING_LEN;
}

/// This is the number of characters a JsVar can contain, NOT string length
static inline size_t jsvGetCharactersInVar(const JsVar *v) {
  assert(jsvIsString(v) || jsvIsStringExt(v));
  int f = v->flags&JSV_VARTYPEMASK;
  return (size_t)(f - ((f < JSV_STRING_EXT_0) ? JSV_STRING_0 : JSV_STRING_EXT_0));
}

/// This is the number of characters a JsVar can contain, NOT string length
static inline void jsvSetCharactersInVar(JsVar *v, size_t chars) {
  assert(jsvIsString(v) || jsvIsStringExt(v));
  if (jsvIsString(v)) { assert(chars <= JSVAR_DATA_STRING_LEN); }
  if (jsvIsStringExt(v)) { assert(chars <= JSVAR_DATA_STRING_MAX_LEN); }

  int f = v->flags&JSV_VARTYPEMASK;
  v->flags = (JsVarFlags)((v->flags&(~JSV_VARTYPEMASK)) | (((f < JSV_STRING_EXT_0) ? JSV_STRING_0 : JSV_STRING_EXT_0) + (int)chars));
}


/** Check if two Basic Variables are equal (this IGNORES the value that is pointed to,
 * so 'a=5'=='a=7' but 'a=5'!='b=5')
 */
bool jsvIsBasicVarEqual(JsVar *a, JsVar *b);

/** Check if two things are equal. Basic vars are done by value,
 * for anything else the reference/pointer must be equal */
bool jsvIsEqual(JsVar *a, JsVar *b);


const char *jsvGetConstString(const JsVar *v); ///< Get a const string representing this variable - if we can. Otherwise return 0
const char *jsvGetTypeOf(const JsVar *v); ///< Return the 'type' of the JS variable (eg. JS's typeof operator)
size_t jsvGetString(const JsVar *v, char *str, size_t len); ///< Save this var as a string to the given buffer, and return how long it was (return val doesn't include terminating 0)
void jsvSetString(JsVar *v, char *str, size_t len); ///< Set the Data in this string. This must JUST overwrite - not extend or shrink
JsVar *jsvAsString(JsVar *var, bool unlockVar); ///< If var is a string, lock and return it, else create a new string
bool jsvIsEmptyString(JsVar *v); ///< Returns true if the string is empty - faster than jsvGetStringLength(v)==0
size_t jsvGetStringLength(JsVar *v); ///< Get the length of this string, IF it is a string
size_t jsvGetLinesInString(JsVar *v); ///<  IN A STRING get the number of lines in the string (min=1)
size_t jsvGetCharsOnLine(JsVar *v, size_t line); ///<  IN A STRING Get the number of characters on a line - lines start at 1
void jsvGetLineAndCol(JsVar *v, size_t charIdx, size_t* line, size_t *col); ///< IN A STRING, get the line and column of the given character. Both values must be non-null
size_t jsvGetIndexFromLineAndCol(JsVar *v, size_t line, size_t col); ///<  IN A STRING, get a character index from a line and column
bool jsvIsStringEqual(JsVar *var, const char *str);
int jsvCompareString(JsVar *va, JsVar *vb, size_t starta, size_t startb, bool equalAtEndOfString); ///< Compare 2 strings, starting from the given character positions
int jsvCompareInteger(JsVar *va, JsVar *vb); ///< Compare 2 integers, >0 if va>vb,  <0 if va<vb. If compared with a non-integer, that gets put later
void jsvAppendString(JsVar *var, const char *str); ///< Append the given string to this one
bool jsvAppendStringBuf(JsVar *var, const char *str, int length); ///< Append the given string to this one - but does not use null-terminated strings. returns false on failure (from out of memory)
void jsvAppendPrintf(JsVar *var, const char *fmt, ...); ///< Append the formatted string to a variable (see vcbprintf)
static inline void jsvAppendCharacter(JsVar *var, char ch) { jsvAppendStringBuf(var, &ch, 1); }; ///< Append the given character to this string
#define JSVAPPENDSTRINGVAR_MAXLENGTH (0x7FFFFFFF)
void jsvAppendStringVar(JsVar *var, const JsVar *str, size_t stridx, size_t maxLength); ///< Append str to var. Both must be strings. stridx = start char or str, maxLength = max number of characters (can be JSVAPPENDSTRINGVAR_MAXLENGTH)
void jsvAppendStringVarComplete(JsVar *var, const JsVar *str); ///< Append all of str to var. Both must be strings.
char jsvGetCharInString(JsVar *v, size_t idx);

JsVarInt jsvGetInteger(const JsVar *v);
void jsvSetInteger(JsVar *v, JsVarInt value); ///< Set an integer value (use carefully!)
JsVarFloat jsvGetFloat(const JsVar *v); ///< Get the floating point representation of this var
bool jsvGetBool(const JsVar *v);
JsVar *jsvAsNumber(JsVar *var); ///< Convert the given variable to a number
static inline JsVarInt _jsvGetIntegerAndUnLock(JsVar *v) { JsVarInt i = jsvGetInteger(v); jsvUnLock(v); return i; }
static inline JsVarFloat _jsvGetFloatAndUnLock(JsVar *v) { JsVarFloat f = jsvGetFloat(v); jsvUnLock(v); return f; }
static inline bool _jsvGetBoolAndUnLock(JsVar *v) { bool b = jsvGetBool(v); jsvUnLock(v); return b; }
#ifdef SAVE_ON_FLASH
JsVarInt jsvGetIntegerAndUnLock(JsVar *v);
JsVarFloat jsvGetFloatAndUnLock(JsVar *v);
bool jsvGetBoolAndUnLock(JsVar *v);
#else
#define jsvGetIntegerAndUnLock _jsvGetIntegerAndUnLock
#define jsvGetFloatAndUnLock _jsvGetFloatAndUnLock
#define jsvGetBoolAndUnLock _jsvGetBoolAndUnLock
#endif


/** Get the item at the given location in the array buffer and return the result */
size_t jsvGetArrayBufferLength(JsVar *arrayBuffer);
/** Get the item at the given location in the array buffer and return the result */
JsVar *jsvArrayBufferGet(JsVar *arrayBuffer, size_t index);
/** Set the item at the given location in the array buffer */
void jsvArrayBufferSet(JsVar *arrayBuffer, size_t index, JsVar *value);
/** Given an integer name that points to an arraybuffer or an arraybufferview, evaluate it and return the result */
JsVar *jsvArrayBufferGetFromName(JsVar *name);

/** If a is a name skip it and go to what it points to - and so on.
 * ALWAYS locks - so must unlock what it returns. It MAY
 * return 0.  */
JsVar *jsvSkipName(JsVar *a);

/** If a is a name skip it and go to what it points to.
 * ALWAYS locks - so must unlock what it returns. It MAY
 * return 0.  */
JsVar *jsvSkipOneName(JsVar *a);

/** If a is a's child is a name skip it and go to what it points to.
 * ALWAYS locks - so must unlock what it returns.  */
JsVar *jsvSkipToLastName(JsVar *a);

/** Same as jsvSkipName, but ensures that 'a' is unlocked if it was
 * a name, so it can be used INLINE_FUNC */
static inline JsVar *jsvSkipNameAndUnLock(JsVar *a) {
  JsVar *b = jsvSkipName(a);
  jsvUnLock(a);
  return b;
}

/** Same as jsvSkipOneName, but ensures that 'a' is unlocked if it was
 * a name, so it can be used INLINE_FUNC */
static inline JsVar *jsvSkipOneNameAndUnLock(JsVar *a) {
  JsVar *b = jsvSkipOneName(a);
  jsvUnLock(a);
  return b;
}

/// MATHS!
JsVar *jsvMathsOpSkipNames(JsVar *a, JsVar *b, int op);
JsVar *jsvMathsOp(JsVar *a, JsVar *b, int op);
/// Negates an integer/double value
JsVar *jsvNegateAndUnLock(JsVar *v);

/// Copy this variable and return the locked copy
JsVar *jsvCopy(JsVar *src);
/** Copy only a name, not what it points to. ALTHOUGH the link to what it points to is maintained unless linkChildren=false.
    If keepAsName==false, this will be converted into a normal variable */
JsVar *jsvCopyNameOnly(JsVar *src, bool linkChildren, bool keepAsName);
/// Tree related stuff
void jsvAddName(JsVar *parent, JsVar *nameChild); // Add a child, which is itself a name
JsVar *jsvAddNamedChild(JsVar *parent, JsVar *child, const char *name); // Add a child, and create a name for it. Returns a LOCKED var. DOES NOT CHECK FOR DUPLICATES
JsVar *jsvSetNamedChild(JsVar *parent, JsVar *child, const char *name); // Add a child, and create a name for it. Returns a LOCKED name var. CHECKS FOR DUPLICATES
JsVar *jsvSetValueOfName(JsVar *name, JsVar *src); // Set the value of a child created with jsvAddName,jsvAddNamedChild. Returns the UNLOCKED name argument
JsVar *jsvFindChildFromString(JsVar *parent, const char *name, bool createIfNotFound); // Non-recursive finding of child with name. Returns a LOCKED var
JsVar *jsvFindChildFromVar(JsVar *parent, JsVar *childName, bool addIfNotFound); // Non-recursive finding of child with name. Returns a LOCKED var
static inline JsVar *jsvFindChildFromStringRef(JsVarRef parentref, const char *name, bool addIfNotFound) { // Non-recursive finding of child with name. Returns a LOCKED var
  JsVar *p = jsvLock(parentref);
  JsVar *v = jsvFindChildFromString(p, name, addIfNotFound);
  jsvUnLock(p);
  return v;
}
static inline JsVar *jsvFindChildFromVarRef(JsVarRef parentref, JsVar *childName, bool addIfNotFound) { // Non-recursive finding of child with name. Returns a LOCKED var
  JsVar *p = jsvLock(parentref);
  JsVar *v = jsvFindChildFromVar(p, childName, addIfNotFound);
  jsvUnLock(p);
  return v;
}
/// Remove a child - note that the child MUST ACTUALLY BE A CHILD! and should be a name, not a value.
void jsvRemoveChild(JsVar *parent, JsVar *child);
void jsvRemoveAllChildren(JsVar *parent);
static inline void jsvRemoveNamedChild(JsVar *parent, const char *name) {
  JsVar *child = jsvFindChildFromString(parent, name, false);
  if (child) {
    jsvRemoveChild(parent, child);
    jsvUnLock(child);
  }
}

/// Get the named child of an object. If createChild!=0 then create the child
JsVar *jsvObjectGetChild(JsVar *obj, const char *name, JsVarFlags createChild);
/// Set the named child of an object, and return the child (so you can choose to unlock it if you want)
JsVar *jsvObjectSetChild(JsVar *obj, const char *name, JsVar *child);

int jsvGetChildren(JsVar *v); ///< number of children of a variable. also see jsvGetArrayLength and jsvGetLength
/// Check if the given name is a child of the parent
bool jsvIsChild(JsVar *parent, JsVar *child);
JsVarInt jsvGetArrayLength(const JsVar *arr); ///< Not the same as GetChildren, as it can be a sparse array
JsVarInt jsvGetLength(JsVar *src); ///< General purpose length function. Does the 'right' thing
size_t jsvCountJsVarsUsed(JsVar *v); ///< Count the amount of JsVars used. Mostly useful for debugging
JsVar *jsvGetArrayItem(JsVar *arr, JsVarInt index); ///< Get an item at the specified index in the array (and lock it)
JsVar *jsvGetArrayIndexOf(JsVar *arr, JsVar *value, bool matchExact); ///< Get the index of the value in the array (matchExact==use pointer, not equality check)
JsVarInt jsvArrayPushWithInitialSize(JsVar *arr, JsVar *value, JsVarInt initialValue); ///< Adds new elements to the end of an array, and returns the new length. initialValue is the item index when no items are currently in the array.
JsVarInt jsvArrayPush(JsVar *arr, JsVar *value); ///< Adds a new element to the end of an array, and returns the new length
JsVarInt jsvArrayPushAndUnLock(JsVar *arr, JsVar *value); ///< Adds a new element to the end of an array, unlocks it, and returns the new length
JsVar *jsvArrayPop(JsVar *arr); ///< Removes the last element of an array, and returns that element (or 0 if empty). includes the NAME
JsVar *jsvArrayPopFirst(JsVar *arr); ///< Removes the first element of an array, and returns that element (or 0 if empty) includes the NAME
JsVar *jsvArrayGetLast(const JsVar *arr); ///< Get the last element of an array (does not remove, unlike jsvArrayPop), and returns that element (or 0 if empty) includes the NAME
JsVar *jsvArrayJoin(JsVar *arr, JsVar *filler); ///< Join all elements of an array together into a string
void jsvArrayInsertBefore(JsVar *arr, JsVar *beforeIndex, JsVar *element); ///< Insert a new element before beforeIndex, DOES NOT UPDATE INDICES
static inline bool jsvArrayIsEmpty(JsVar *arr) { assert(jsvIsArray(arr)); return !arr->firstChild; } ///< Return true is array is empty

/** Write debug info for this Var out to the console */
void jsvTrace(JsVarRef ref, int indent);

/** Run a garbage collection sweep - return true if things have been freed */
bool jsvGarbageCollect();

/** Remove whitespace to the right of a string - on MULTIPLE LINES */
JsVar *jsvStringTrimRight(JsVar *srcString);

/** If v is the key of a function, return true if it is internal and shouldn't be visible to the user */
bool jsvIsInternalFunctionKey(JsVar *v);

/// If v is the key of an object, return true if it is internal and shouldn't be visible to the user
bool jsvIsInternalObjectKey(JsVar *v);
// --------------------------------------------------------------------------------------------
typedef struct JsvStringIterator {
  size_t charIdx; ///< index of character in var
  size_t charsInVar; ///< total characters in var
  size_t varIndex; ///< index in string of the start of this var
  JsVar *var; ///< current StringExt we're looking at
} JsvStringIterator;

// slight hack to enure we can use string iterator with const JsVars
#define jsvStringIteratorNewConst(it,str,startIdx) jsvStringIteratorNew(it,(JsVar*)str,startIdx)

/// Create a new String iterator from a string, starting from a specific character. NOTE: This does not keep a lock to the first element, so make sure you do or the string will be freed!
void jsvStringIteratorNew(JsvStringIterator *it, JsVar *str, size_t startIdx);

/// Clone the string iterator
static inline JsvStringIterator jsvStringIteratorClone(JsvStringIterator *it) {
  JsvStringIterator i = *it;
  if (i.var) jsvLockAgain(i.var);
  return i;
}


/// Gets the current character (or 0)
static inline char jsvStringIteratorGetChar(JsvStringIterator *it) {
  if (!it->var) return 0;
  return  it->var->varData.str[it->charIdx];
}

/// Gets the current (>=0) character (or -1)
static inline int jsvStringIteratorGetCharOrMinusOne(JsvStringIterator *it) {
  if (!it->var) return -1;
  return (int)(unsigned char)it->var->varData.str[it->charIdx];
}

/// Do we have a character, or are we at the end?
static inline bool jsvStringIteratorHasChar(JsvStringIterator *it) {
  return it->charIdx < it->charsInVar;
}

/// Sets a character (will not extend the string - just overwrites)
static inline void jsvStringIteratorSetChar(JsvStringIterator *it, char c) {
  if (jsvStringIteratorHasChar(it))
    it->var->varData.str[it->charIdx] = c;
}

/// Gets the current index in the string
static inline size_t jsvStringIteratorGetIndex(JsvStringIterator *it) {
  return it->varIndex + it->charIdx;
}

/// Move to next character
void jsvStringIteratorNext(JsvStringIterator *it);

/// Move to next character (this one is inlined where speed is needed)
static inline void jsvStringIteratorNextInline(JsvStringIterator *it) {
  it->charIdx++;
  if (it->charIdx >= it->charsInVar) {
    it->charIdx -= it->charsInVar;
    if (it->var && it->var->lastChild) {
      JsVar *next = jsvLock(it->var->lastChild);
      jsvUnLock(it->var);
      it->var = next;
      it->varIndex += it->charsInVar;
      it->charsInVar = jsvGetCharactersInVar(it->var);
    } else {
      jsvUnLock(it->var);
      it->var = 0;
      it->varIndex += it->charsInVar;
      it->charsInVar = 0;
    }
  }
}


/// Go to the end of the string iterator - for use with jsvStringIteratorAppend
void jsvStringIteratorGotoEnd(JsvStringIterator *it);

/// Append a character TO THE END of a string iterator
void jsvStringIteratorAppend(JsvStringIterator *it, char ch);

static inline void jsvStringIteratorFree(JsvStringIterator *it) {
  jsvUnLock(it->var);
}

/// Special version of append designed for use with vcbprintf_callback (See jsvAppendPrintf)
void jsvStringIteratorPrintfCallback(const char *str, void *user_data);

// --------------------------------------------------------------------------------------------
typedef struct JsvArrayIterator {
  JsVar *var;
} JsvArrayIterator;

static inline void jsvArrayIteratorNew(JsvArrayIterator *it, JsVar *arr) {
  assert(jsvIsArray(arr));
  it->var = arr->firstChild ? jsvLock(arr->firstChild) : 0;
}

/// Clone the iterator
static inline JsvArrayIterator jsvArrayIteratorClone(JsvArrayIterator *it) {
  JsvArrayIterator i = *it;
  if (i.var) jsvLockAgain(i.var);
  return i;
}

/// Gets the current array element (or 0)
static inline JsVar *jsvArrayIteratorGetElement(JsvArrayIterator *it) {
  if (!it->var) return 0; // end of array
  return it->var->firstChild ? jsvLock(it->var->firstChild) : 0; // might even be undefined
}

/// Set the current array element
static inline void jsvArrayIteratorSetElement(JsvArrayIterator *it, JsVar *value) {
  if (!it->var) return; // end of array
  jsvSetValueOfName(it->var, value);
}

/// Gets the current array element index (or 0)
static inline JsVar *jsvArrayIteratorGetIndex(JsvArrayIterator *it) {
  if (!it->var) return 0;
  return jsvLockAgain(it->var);
}

/// Do we have a character, or are we at the end?
static inline bool jsvArrayIteratorHasElement(JsvArrayIterator *it) {
  return it->var != 0;
}

/// Move to next element
static inline void jsvArrayIteratorNext(JsvArrayIterator *it) {
  if (it->var) {
    JsVarRef next = it->var->nextSibling;
    jsvUnLock(it->var);
    it->var = next ? jsvLock(next) : 0;
  }
}

/// Remove the current element and move to next element. Needs the parent supplied (the JsVar passed to jsvArrayIteratorNew) as we don't store it
static inline void jsvArrayIteratorRemoveAndGotoNext(JsvArrayIterator *it, JsVar *parent) {
  if (it->var) {
    JsVarRef next = it->var->nextSibling;
    jsvRemoveChild(parent, it->var);
    jsvUnLock(it->var);
    it->var = next ? jsvLock(next) : 0;
  }
}

static inline void jsvArrayIteratorFree(JsvArrayIterator *it) {
  jsvUnLock(it->var);
}
// --------------------------------------------------------------------------------------------
typedef struct JsvObjectIterator {
  JsVar *var;
} JsvObjectIterator;

static inline void jsvObjectIteratorNew(JsvObjectIterator *it, JsVar *obj) {
  assert(jsvIsObject(obj) || jsvIsFunction(obj));
  it->var = obj->firstChild ? jsvLock(obj->firstChild) : 0;
}

/// Clone the iterator
static inline JsvObjectIterator jsvObjectIteratorClone(JsvObjectIterator *it) {
  JsvObjectIterator i = *it;
  if (i.var) jsvLockAgain(i.var);
  return i;
}

/// Gets the current object element key (or 0)
static inline JsVar *jsvObjectIteratorGetKey(JsvObjectIterator *it) {
  if (!it->var) return 0; // end of object
  return jsvLockAgain(it->var);
}

/// Gets the current object element value (or 0)
static inline JsVar *jsvObjectIteratorGetValue(JsvObjectIterator *it) {
  if (!it->var) return 0; // end of object
  return it->var->firstChild ? jsvLock(it->var->firstChild) : 0; // might even be undefined
}

/// Set the current array element
static inline void jsvObjectIteratorSetValue(JsvObjectIterator *it, JsVar *value) {
  if (!it->var) return; // end of object
  jsvSetValueOfName(it->var, value);
}


/// Do we have a key, or are we at the end?
static inline bool jsvObjectIteratorHasElement(JsvObjectIterator *it) {
  return it->var != 0;
}

/// Move to next character
static inline void jsvObjectIteratorNext(JsvObjectIterator *it) {
  if (it->var) {
    JsVarRef next = it->var->nextSibling;
    jsvUnLock(it->var);
    it->var = next ? jsvLock(next) : 0;
  }
}

static inline void jsvObjectIteratorFree(JsvObjectIterator *it) {
  jsvUnLock(it->var);
}
// --------------------------------------------------------------------------------------------
typedef struct JsvArrayBufferIterator {
  JsvStringIterator it;
  JsVarDataArrayBufferViewType type;
  size_t byteLength;
  size_t byteOffset;
  size_t index;
  bool hasAccessedElement;
} JsvArrayBufferIterator;

void   jsvArrayBufferIteratorNew(JsvArrayBufferIterator *it, JsVar *arrayBuffer, size_t index);

/// Clone the iterator
static inline JsvArrayBufferIterator jsvArrayBufferIteratorClone(JsvArrayBufferIterator *it) {
  JsvArrayBufferIterator i = *it;
  i.it = jsvStringIteratorClone(&it->it);
  return i;
}

/** ArrayBuffers have the slightly odd side-effect that you can't write an element
 * once you have read it. That's why we have jsvArrayBufferIteratorGetValueAndRewind
 * which allows this, but is slower. */

JsVar *jsvArrayBufferIteratorGetValue(JsvArrayBufferIterator *it);
JsVar *jsvArrayBufferIteratorGetValueAndRewind(JsvArrayBufferIterator *it);
JsVarInt jsvArrayBufferIteratorGetIntegerValue(JsvArrayBufferIterator *it);
JsVarFloat jsvArrayBufferIteratorGetFloatValue(JsvArrayBufferIterator *it);
void   jsvArrayBufferIteratorSetValue(JsvArrayBufferIterator *it, JsVar *value);
void   jsvArrayBufferIteratorSetIntegerValue(JsvArrayBufferIterator *it, JsVarInt value);
JsVar* jsvArrayBufferIteratorGetIndex(JsvArrayBufferIterator *it);
bool   jsvArrayBufferIteratorHasElement(JsvArrayBufferIterator *it);
void   jsvArrayBufferIteratorNext(JsvArrayBufferIterator *it);
void   jsvArrayBufferIteratorFree(JsvArrayBufferIterator *it);
// --------------------------------------------------------------------------------------------
union JsvIteratorUnion {
  JsvStringIterator str;
  JsvObjectIterator obj;
  JsvArrayIterator arr;
  JsvArrayBufferIterator buf;
};

/** General Purpose iterator, for Strings, Arrays, Objects, Typed Arrays */
typedef struct JsvIterator {
  enum {JSVI_STRING, JSVI_ARRAY, JSVI_OBJECT, JSVI_ARRAYBUFFER } type;
  union JsvIteratorUnion it;
} JsvIterator;

void jsvIteratorNew(JsvIterator *it, JsVar *obj);
JsVar *jsvIteratorGetKey(JsvIterator *it);
JsVar *jsvIteratorGetValue(JsvIterator *it);
JsVarInt jsvIteratorGetIntegerValue(JsvIterator *it);
JsVarFloat jsvIteratorGetFloatValue(JsvIterator *it);
JsVar *jsvIteratorSetValue(JsvIterator *it, JsVar *value); // set the value - return it in case we need to unlock it, eg. jsvUnLock(jsvIteratorSetValue(&it, jsvNew...));
bool jsvIteratorHasElement(JsvIterator *it);
void jsvIteratorNext(JsvIterator *it);
void jsvIteratorFree(JsvIterator *it);
JsvIterator jsvIteratorClone(JsvIterator *it);



#endif /* JSVAR_H_ */
