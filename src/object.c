#include <stdio.h>
#include <string.h>

#include "../headers/memory.h"
#include "../headers/object.h"
#include "../headers/table.h"
#include "../headers/value.h"
#include "../headers/vm.h"

static Obj *allocateObject(size_t size, ObjType type) {
  Obj *object = (Obj *)reallocate(NULL, 0, size);
  object->type = type;
  object->forwardingAddress = NULL; // Initialize forwarding address
#ifdef DEBUG_LOG_GC
  printf("%p allocate %zu for %d\n", (void *)object, size, type);
#endif
  return object;
}

ObjBoundMethod *newBoundMethod(Value receiver, ObjClosure *method) {
  ObjBoundMethod *bound = (ObjBoundMethod *)allocateObject(
      sizeof(ObjBoundMethod), OBJ_BOUND_METHOD);
  bound->receiver = receiver;
  bound->method = method;
  return bound;
}

ObjClass *newClass(ObjString *name) {
  ObjClass *klass = (ObjClass *)allocateObject(sizeof(ObjClass), OBJ_CLASS);
  klass->name = name;
  initTable(&klass->methods);
  return klass;
}

ObjClosure *newClosure(ObjFunction *function) {
  // Allocate space for the closure and its upvalue array in one block.
  size_t upvaluesSize = sizeof(ObjUpvalue *) * function->upvalueCount;
  size_t totalSize = sizeof(ObjClosure) + upvaluesSize;
  ObjClosure *closure = (ObjClosure *)allocateObject(totalSize, OBJ_CLOSURE);

  closure->function = function;
  closure->upvalueCount = function->upvalueCount;
  closure->upvalues =
      (ObjUpvalue **)(closure + 1); // Point to the memory after the struct

  for (int i = 0; i < function->upvalueCount; i++) {
    closure->upvalues[i] = NULL;
  }
  return closure;
}

ObjFunction *newFunction() {
  ObjFunction *function =
      (ObjFunction *)allocateObject(sizeof(ObjFunction), OBJ_FUNCTION);
  function->arity = 0;
  function->upvalueCount = 0;
  function->name = NULL;
  initChunk(&function->chunk);
  return function;
}

ObjInstance *newInstance(ObjClass *klass) {
  ObjInstance *instance =
      (ObjInstance *)allocateObject(sizeof(ObjInstance), OBJ_INSTANCE);
  instance->klass = klass;
  initTable(&instance->fields);
  return instance;
}

ObjNative *newNative(NativeFn function) {
  ObjNative *native =
      (ObjNative *)allocateObject(sizeof(ObjNative), OBJ_NATIVE);
  native->function = function;
  return native;
}

static uint32_t hashString(const char *key, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}

// Unified string allocation function.
static ObjString *allocateString(const char *chars, int length, uint32_t hash) {
  ObjString *interned = tableFindString(&vm.strings, chars, length, hash);
  if (interned != NULL) {
    return interned;
  }

  size_t totalSize = sizeof(ObjString) + length + 1;
  ObjString *string = (ObjString *)allocateObject(totalSize, OBJ_STRING);
  string->length = length;
  string->hash = hash;
  string->chars = (char *)(string + 1); // Point to memory after the struct
  memcpy(string->chars, chars, length);
  string->chars[length] = '\0';

  push(OBJ_VAL(string));
  tableSet(&vm.strings, string, NIL_VAL);
  pop();

  return string;
}

ObjString *copyString(const char *chars, int length) {
  uint32_t hash = hashString(chars, length);
  return allocateString(chars, length, hash);
}

ObjString *takeString(char *chars, int length) {
  uint32_t hash = hashString(chars, length);
  // We can't "take" the string anymore, we must copy it into the heap.
  ObjString *result = allocateString(chars, length, hash);
  // Free the original, now-unused buffer.
  FREE_ARRAY(char, chars, length + 1);
  return result;
}

ObjUpvalue *newUpvalue(Value *slot) {
  ObjUpvalue *upvalue =
      (ObjUpvalue *)allocateObject(sizeof(ObjUpvalue), OBJ_UPVALUE);
  upvalue->closed = NIL_VAL;
  upvalue->location = slot;
  upvalue->next = NULL;
  return upvalue;
}

static void printFunction(ObjFunction *function) {
  if (function->name == NULL) {
    printf("<script>");
    return;
  }
  printf("<fn %s>", function->name->chars);
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
  case OBJ_BOUND_METHOD:
    printFunction(AS_BOUND_METHOD(value)->method->function);
    break;
  case OBJ_CLASS:
    printf("%s", AS_CLASS(value)->name->chars);
    break;
  case OBJ_CLOSURE:
    printFunction(AS_CLOSURE(value)->function);
    break;
  case OBJ_FUNCTION:
    printFunction(AS_FUNCTION(value));
    break;
  case OBJ_INSTANCE:
    printf("%s instance", AS_INSTANCE(value)->klass->name->chars);
    break;
  case OBJ_NATIVE:
    printf("<native fn>");
    break;
  case OBJ_STRING:
    printf("%s", AS_CSTRING(value));
    break;
  case OBJ_UPVALUE:
    printf("upvalue");
    break;
  }
}
