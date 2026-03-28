#ifndef FORWARD_H
#define FORWARD_H

#define FORWARD_DECLARE(name) struct name; typedef struct name name;

typedef enum {
    INPUTPRESS,
    INPUTRELEASE,
    INPUTDOWN,
} InputAction;

FORWARD_DECLARE(World);
FORWARD_DECLARE(Entity);
FORWARD_DECLARE(System);

#endif
