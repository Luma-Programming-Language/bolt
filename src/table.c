#include <string.h>

#include "../headers/memory.h"
#include "../headers/table.h"
#include "../headers/value.h"

#define TABLE_MAX_LOAD 0.75

void initTable(Table *table) {
    table->count = 0;
    table->capacityMask = -1; // Use -1 to indicate 0 capacity
    table->entries = NULL;
}

void freeTable(Table *table) {
    FREE_ARRAY(Entry, table->entries, table->capacityMask + 1);
    initTable(table);
}

static Entry *findEntry(Entry *entries, int capacityMask, ObjString *key) {
    uint32_t index = key->hash & capacityMask;
    Entry *tombstone = NULL;

    for (;;) {
        Entry *entry = &entries[index];
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                // Empty entry.
                return tombstone != NULL ? tombstone : entry;
            } else {
                // We found a tombstone.
                if (tombstone == NULL)
                    tombstone = entry;
            }
        } else if (entry->key == key) {
            // We found the key.
            return entry;
        }
        index = (index + 1) & capacityMask;
    }
}

bool tableGet(Table *table, ObjString *key, Value *value) {
    if (table->count == 0)
        return false;

    Entry *entry = findEntry(table->entries, table->capacityMask, key);
    if (entry->key == NULL)
        return false;

    *value = entry->value;
    return true;
}

static void adjustCapacity(Table *table, int capacity) {
    Entry *entries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    table->count = 0;
    for (int i = 0; i <= table->capacityMask; i++) {
        Entry *entry = &table->entries[i];
        if (entry->key == NULL)
            continue;

        Entry *dest = findEntry(entries, capacity - 1, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    FREE_ARRAY(Entry, table->entries, table->capacityMask + 1);
    table->entries = entries;
    table->capacityMask = capacity - 1;
}

bool tableSet(Table *table, ObjString *key, Value value) {
    if (table->count + 1 > (table->capacityMask + 1) * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacityMask + 1);
        adjustCapacity(table, capacity);
    }

    Entry *entry = findEntry(table->entries, table->capacityMask, key);
    bool isNewKey = entry->key == NULL;
    if (isNewKey && IS_NIL(entry->value))
        table->count++;

    entry->key = key;
    entry->value = value;
    return isNewKey;
}

bool tableDelete(Table *table, ObjString *key) {
    if (table->count == 0)
        return false;

    Entry *entry = findEntry(table->entries, table->capacityMask, key);
    if (entry->key == NULL)
        return false;

    // Place a tombstone in the entry.
    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    return true;
}

void tableAddAll(Table *from, Table *to) {
    for (int i = 0; i <= from->capacityMask; i++) {
        Entry *entry = &from->entries[i];
        if (entry->key != NULL) {
            tableSet(to, entry->key, entry->value);
        }
    }
}

ObjString *tableFindString(Table *table, const char *chars, int length,
                           uint32_t hash) {
    if (table->count == 0)
        return NULL;

    uint32_t index = hash & table->capacityMask;
    for (;;) {
        Entry *entry = &table->entries[index];
        if (entry->key == NULL) {
            // Stop if we find an empty, non-tombstone entry.
            if (IS_NIL(entry->value))
                return NULL;
        } else if (entry->key->length == length && entry->key->hash == hash &&
                   memcmp(entry->key->chars, chars, length) == 0) {
            // We found it.
            return entry->key;
        }

        index = (index + 1) & table->capacityMask;
    }
}

// Updated to work with the new GC's forwarding pointers.
void tableRemoveWhite(Table *table) {
    for (int i = 0; i <= table->capacityMask; i++) {
        Entry *entry = &table->entries[i];
        // A "white" object is one that was not marked. In our new GC,
        // unmarked objects have a NULL forwardingAddress.
        if (entry->key != NULL && entry->key->obj.forwardingAddress == NULL) {
            tableDelete(table, entry->key);
        }
    }
}

void markTable(Table *table) {
    for (int i = 0; i <= table->capacityMask; i++) {
        Entry *entry = &table->entries[i];
        markObject((Obj *)entry->key);
        markValue(entry->value);
    }
}

// New function to update pointers during the compaction phase.
void tableUpdatePointers(Table *table) {
    for (int i = 0; i <= table->capacityMask; i++) {
        Entry *entry = &table->entries[i];
        if (entry->key != NULL) {
            // Update the key pointer to its new location
            entry->key = (ObjString *)entry->key->obj.forwardingAddress;
            // Update the value pointer if it's an object
            if (IS_OBJ(entry->value)) {
                entry->value = OBJ_VAL(AS_OBJ(entry->value)->forwardingAddress);
            }
        }
    }
}
