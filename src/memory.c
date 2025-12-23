#include <stdlib.h>
#include <string.h>

#include "../headers/compiler.h"
#include "../headers/memory.h"
#include "../headers/vm.h"
#include <stdio.h>

#ifdef DEBUG_LOG_GC
#include "../headers/debug.h"

#endif

#define GC_HEAP_GROW_FACTOR 2

// The new bump allocator.
void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
  vm.bytesAllocated += newSize - oldSize;
  if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
    collectGarbage();
#endif
    if (vm.bytesAllocated > vm.nextGC) {
      collectGarbage();
    }
  }

  if (newSize == 0) {
    // Individual frees do nothing in a compacting collector.
    return NULL;
  }

  // Check if there is enough space on the heap.
  if ((char *)vm.next + newSize > (char *)vm.heap + vm.heapCapacity) {
    fprintf(stderr, "Error: Out of memory.\n");
    exit(1);
  }

  void *result = vm.next;
  vm.next = (char *)vm.next + newSize;

  // For GROW_ARRAY, we allocate a new block and copy the old data.
  // The old block becomes garbage, to be collected later.
  if (pointer != NULL) {
    memcpy(result, pointer, oldSize);
  }

  return result;
}

// Helper to calculate the size of an object on the heap.
static size_t sizeOfObject(Obj *object) {
  switch (object->type) {
  case OBJ_BOUND_METHOD:
    return sizeof(ObjBoundMethod);
  case OBJ_CLASS:
    return sizeof(ObjClass);
  case OBJ_CLOSURE: {
    ObjClosure *closure = (ObjClosure *)object;
    return sizeof(ObjClosure) + sizeof(ObjUpvalue *) * closure->upvalueCount;
  }
  case OBJ_FUNCTION:
    return sizeof(ObjFunction);
  case OBJ_INSTANCE:
    return sizeof(ObjInstance);
  case OBJ_NATIVE:
    return sizeof(ObjNative);
  case OBJ_STRING: {
    ObjString *string = (ObjString *)object;
    return sizeof(ObjString) + string->length + 1;
  }
  case OBJ_UPVALUE:
    return sizeof(ObjUpvalue);
  default:
    return 0; // Should not happen.
  }
}

// --- GC Marking Phase ---

void markObject(Obj *object) {
  if (object == NULL || object->forwardingAddress != NULL) {
    return; // Already marked or null.
  }
  // Mark by pointing to itself.
  object->forwardingAddress = object;
#ifdef DEBUG_LOG_GC
  printf("%p mark ", (void *)object);
  printValue(OBJ_VAL(object));
  printf("\n");
#endif
}

void markValue(Value value) {
  if (IS_OBJ(value))
    markObject(AS_OBJ(value));
}

static void markArray(ValueArray *array) {
  for (int i = 0; i < array->count; i++) {
    markValue(array->values[i]);
  }
}

static void traceObject(Obj *object) {
#ifdef DEBUG_LOG_GC
  printf("%p trace ", (void *)object);
  printValue(OBJ_VAL(object));
  printf("\n");
#endif
  switch (object->type) {
  case OBJ_BOUND_METHOD: {
    ObjBoundMethod *bound = (ObjBoundMethod *)object;
    markValue(bound->receiver);
    markObject((Obj *)bound->method);
    break;
  }
  case OBJ_CLASS: {
    ObjClass *klass = (ObjClass *)object;
    markObject((Obj *)klass->name);
    markTable(&klass->methods);
    break;
  }
  case OBJ_CLOSURE: {
    ObjClosure *closure = (ObjClosure *)object;
    markObject((Obj *)closure->function);
    for (int i = 0; i < closure->upvalueCount; i++) {
      markObject((Obj *)closure->upvalues[i]);
    }
    break;
  }
  case OBJ_FUNCTION: {
    ObjFunction *function = (ObjFunction *)object;
    markObject((Obj *)function->name);
    markArray(&function->chunk.constants);
    break;
  }
  case OBJ_INSTANCE: {
    ObjInstance *instance = (ObjInstance *)object;
    markObject((Obj *)instance->klass);
    markTable(&instance->fields);
    break;
  }
  case OBJ_UPVALUE:
    markValue(((ObjUpvalue *)object)->closed);
    break;
  case OBJ_NATIVE:
  case OBJ_STRING:
    break;
  }
}

static void markRoots() {
  for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
    markValue(*slot);
  }
  for (int i = 0; i < vm.frameCount; i++) {
    markObject((Obj *)vm.frames[i].closure);
  }
  for (ObjUpvalue *upvalue = vm.openUpvalues; upvalue != NULL;
       upvalue = upvalue->next) {
    markObject((Obj *)upvalue);
  }
  markTable(&vm.globals);
  markCompilerRoots();
  markObject((Obj *)vm.initString);
}

// --- GC Compaction Phase ---

static size_t calculateNewLocations() {
  void *to = vm.heap;
  void *from = vm.heap;
  while (from < vm.next) {
    Obj *object = (Obj *)from;
    size_t size = sizeOfObject(object);
    if (object->forwardingAddress != NULL) { // If it's a live object
      object->forwardingAddress = to;
      to = (char *)to + size;
    }
    from = (char *)from + size;
  }
  return (char *)to - (char *)vm.heap; // Total size of live objects
}

static void updatePtr(Obj **p) {
  if (*p != NULL) {
    *p = (*p)->forwardingAddress;
  }
}

static void updateObjectPointers(Obj *object) {
  switch (object->type) {
  case OBJ_BOUND_METHOD: {
    ObjBoundMethod *bound = (ObjBoundMethod *)object;
    if (IS_OBJ(bound->receiver))
      bound->receiver = OBJ_VAL(AS_OBJ(bound->receiver)->forwardingAddress);
    updatePtr((Obj **)&bound->method);
    break;
  }
  case OBJ_CLASS: {
    ObjClass *klass = (ObjClass *)object;
    updatePtr((Obj **)&klass->name);
    tableUpdatePointers(&klass->methods);
    break;
  }
  case OBJ_CLOSURE: {
    ObjClosure *closure = (ObjClosure *)object;
    updatePtr((Obj **)&closure->function);
    for (int i = 0; i < closure->upvalueCount; i++) {
      updatePtr((Obj **)&closure->upvalues[i]);
    }
    break;
  }
  case OBJ_FUNCTION: {
    ObjFunction *function = (ObjFunction *)object;
    updatePtr((Obj **)&function->name);
    // update constants table in chunk
    break;
  }
  case OBJ_INSTANCE: {
    ObjInstance *instance = (ObjInstance *)object;
    updatePtr((Obj **)&instance->klass);
    tableUpdatePointers(&instance->fields);
    break;
  }
  // Other types don't have internal object pointers to update
  default:
    break;
  }
}

static void updatePointers() {
  // Update roots
  for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
    if (IS_OBJ(*slot)) {
      *slot = OBJ_VAL(AS_OBJ(*slot)->forwardingAddress);
    }
  }
  for (int i = 0; i < vm.frameCount; i++) {
    updatePtr((Obj **)&vm.frames[i].closure);
  }
  for (ObjUpvalue *upvalue = vm.openUpvalues; upvalue != NULL;
       upvalue = upvalue->next) {
    updatePtr((Obj **)&upvalue);
  }
  tableUpdatePointers(&vm.globals);
  // markCompilerRoots(); // needs update logic too
  updatePtr((Obj **)&vm.initString);

  // Update pointers within heap objects
  void *from = vm.heap;
  while (from < vm.next) {
    Obj *object = (Obj *)from;
    if (object->forwardingAddress != NULL) { // Is live
      updateObjectPointers(object);
    }
    from = (char *)from + sizeOfObject(object);
  }
}

static void compactHeap() {
  void *from = vm.heap;
  while (from < vm.next) {
    Obj *object = (Obj *)from;
    size_t size = sizeOfObject(object);
    if (object->forwardingAddress != NULL) { // Is live
      void *to = object->forwardingAddress;
      if (to != from) {
        memmove(to, from, size);
      }
      // Clear the mark for the next GC cycle.
      ((Obj *)to)->forwardingAddress = NULL;
    }
    from = (char *)from + size;
  }
}

void collectGarbage() {
#ifdef DEBUG_LOG_GC
  printf("-- gc begin\n");
  size_t before = vm.bytesAllocated;
#endif

  // Phase 1: Mark all reachable objects.
  markRoots();
  // Trace references until all are marked. We need to iterate until no more
  // marks are added. A simple way is to re-scan the heap.
  bool changed = true;
  while (changed) {
    changed = false;
    void *p = vm.heap;
    while (p < vm.next) {
      Obj *o = (Obj *)p;
      if (o->forwardingAddress == o) { // Marked but not yet traced
        o->forwardingAddress = NULL;   // Un-trace
        markObject(o);                 // Re-mark
        traceObject(o);                // And trace
        changed = true;
      }
      p = (char *)p + sizeOfObject(o);
    }
  }

  tableRemoveWhite(&vm.strings);

  // Phase 2: Calculate new locations for live objects.
  size_t liveSize = calculateNewLocations();

  // Phase 3: Update all pointers to refer to the new locations.
  updatePointers();

  // Phase 4: Compact the heap by moving objects.
  compactHeap();

  // Update VM state.
  vm.next = (char *)vm.heap + liveSize;
  vm.bytesAllocated = liveSize;
  vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
  printf("-- gc end\n");
  printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
         before - vm.bytesAllocated, before, vm.bytesAllocated, vm.nextGC);
#endif
}

// function not needed - heap freed all at once
void freeObjects() {}
