#ifndef INPUT_TYPES_H
#define INPUT_TYPES_H

/*
 * Semantic action set -- the only actions game logic ever sees.
 * Raw input events (SDL scancodes, gamepad buttons, touch) are
 * mapped to these by platform-specific input adapters.
 */
typedef enum {
    ACTION_NONE = 0,

    /* UI / menu navigation */
    ACTION_UI_UP,
    ACTION_UI_DOWN,
    ACTION_UI_LEFT,
    ACTION_UI_RIGHT,
    ACTION_UI_CONFIRM,
    ACTION_UI_BACK,

    /* Puzzle gameplay */
    ACTION_MOVE_NORTH,
    ACTION_MOVE_SOUTH,
    ACTION_MOVE_EAST,
    ACTION_MOVE_WEST,
    ACTION_WAIT,
    ACTION_UNDO,
    ACTION_RESET,
    ACTION_PAUSE,

    /* Overworld navigation */
    ACTION_OW_MOVE_NORTH,
    ACTION_OW_MOVE_SOUTH,
    ACTION_OW_MOVE_EAST,
    ACTION_OW_MOVE_WEST,
    ACTION_OW_ENTER,
    ACTION_OW_BACK,

    /* Debug / development */
    ACTION_DEBUG_TOGGLE_CAMERA,      /* 'C' — switch between 2D and 3D rendering */
    ACTION_DEBUG_TOGGLE_PROJECTION,  /* 'V' — switch between ortho and perspective */
    ACTION_DEBUG_PITCH_UP,           /* 'I' — increase camera pitch */
    ACTION_DEBUG_PITCH_DOWN,         /* 'K' — decrease camera pitch */
    ACTION_DEBUG_FOV_UP,             /* 'O' — increase FOV */
    ACTION_DEBUG_FOV_DOWN,           /* 'L' — decrease FOV */
    ACTION_DEBUG_ANIM_SPEED,         /* 'P' — cycle animation speed multiplier */

    ACTION_COUNT
} SemanticAction;

/*
 * Input context determines how raw input maps to semantic actions.
 * The input manager uses this to select the correct mapping table.
 */
typedef enum {
    INPUT_CONTEXT_MENU,
    INPUT_CONTEXT_PUZZLE,
    INPUT_CONTEXT_OVERWORLD
} InputContext;

#endif /* INPUT_TYPES_H */
