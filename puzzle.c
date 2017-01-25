/*
  Try to find a tight fit (4x7) of the 7 tetris pieces.
*/

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


#define NUM_PIECES 7
#define W 4
#define H 7

//#define VERBOSE 1


/* Definition of the pieces. */
struct piece_def {
  const char *def;
  uint8_t h, w;
};

static const struct piece_def piece_definition[NUM_PIECES] = {
  { "####", 1, 4 },

  { "##"
    "##", 2, 2 },

  { "## "
    " ##", 2, 3 },

  { " ##"
    "## ", 2, 3 },

  { "###"
    "#  ", 2, 3 },

  { "###"
    "  #", 2, 3 },

  { " # "
    "###", 2, 3 }
};


/*
  Efficient representation of all the possible orientations of the different
  pieces. Computed automatically from piece_definition[].

  A piece is stored as a bitmask (4x7 pieces fit in 28 < 32 bit).
  Position (X, Y) maps to bit (X+4*Y).
*/
struct piece_orientations {
  uint32_t n;                            /* Number of distinct orientations */
  uint32_t masks[8];                     /* Max. 4 rotations times 2 flips */
  uint32_t w[8];                         /* Width of this orientation */
  uint32_t h[8];                         /* Height of this orientation */
};


/* Record placement of a piece, to eventually display any found solutions. */
struct placement {
  uint32_t piece_mask;
};


__attribute__((unused))
static void
print_piece(uint32_t mask, uint32_t w, uint32_t h)
{
  uint32_t x, y;

  for (y = 0; y < h; ++y) {
    for (x = 0; x < w; ++x) {
      printf("%c", (mask & (1 << (W*y+x)) ? '#' : ' '));
    }
    printf("\n");
  }
  printf("--------\n");
}


__attribute__((unused))
static void
print_solution(struct placement solution[NUM_PIECES], uint32_t sofar)
{
  static const char patterns[NUM_PIECES+1] = ".%=#@$*";
  uint32_t i, x, y;
  for (y = 0; y < H; ++y) {
    for (x = 0; x < W; ++x) {
      uint32_t found = 0;
      for (i = 0; i < sofar; ++i) {
        uint32_t mask = solution[i].piece_mask;
        if (mask & (1 << (x + W*y))) {
          if (found) {
            fprintf(stderr, "Internal error: overlapping pieces!\n");
            exit(1);
          }
          printf("%c", patterns[i]);
          found = 1;
        }
      }
      if (!found)
        printf(" ");
    }
    printf("\n");
  }
  printf("--------\n");
}


__attribute__((unused))
static void
print_state(uint32_t state)
{
  print_piece(state, W, H);
}


/*
  From the piece definitions, compute all the possible orientations of each
  piece.

  There are 8 potential orientations - 4 rotations, each mirrored or not.
  However, symmetry will mean that many of those potential orientations are
  identical, and thus redundant to check.
*/
static void
calc_pieces_available(struct piece_orientations avail[NUM_PIECES])
{
  uint32_t i, j, k, l;

  for (i = 0; i < NUM_PIECES; ++i) {
    uint32_t piece_w = piece_definition[i].w;
    uint32_t piece_h = piece_definition[i].h;
    const char *def = piece_definition[i].def;
    uint32_t x, y;
    uint32_t mask;
    uint32_t idx;

    /* Compute the piece mask from the definition string. */
    mask = 0;
    for (y = 0; y < piece_h; ++y) {
      for (x = 0; x < piece_w; ++x) {
        if (def[x+piece_w*y] != ' ')
          mask |= 1 << (x+W*y);
      }
    }

    idx = 0;
    /* Loop over 4 rotations. */
    for (j = 0; j < 4; ++j) {
      uint32_t rotated_mask, rotated_width, rotated_height;

      /* Loop over {not mirrored, mirrored}. */
      for (k = 0; k < 2; ++k) {
        /* Check for redundant orientation. */
        for (l = 0; l < idx; ++l) {
          if (avail[i].w[l] == piece_w &&
              avail[i].h[l] == piece_h &&
              avail[i].masks[l] == mask)
            goto redundant_orientation;
        }
        /* Add this as a new available orientation. */
        avail[i].masks[idx] = mask;
        avail[i].w[idx] = piece_w;
        avail[i].h[idx] = piece_h;
#ifdef VERBOSE
        print_piece(avail[i].masks[idx], avail[i].w[idx], avail[i].h[idx]);
#endif
        ++idx;
    redundant_orientation:

        /* Calculate the mirror-image of this orientation. */
        for (l = 0; l < piece_h/2; ++l) {
          static const uint32_t horiz_mask = (1<<W)-1;
          uint32_t bot_shift = W*l;
          uint32_t top_shift = W*((piece_h-1)-l);
          uint32_t bot_mask = horiz_mask << bot_shift;
          uint32_t top_mask = horiz_mask << top_shift;
          uint32_t bot = (mask & bot_mask) >> bot_shift;
          uint32_t top = (mask & top_mask) >> top_shift;
          mask = (mask & ~(bot_mask | top_mask)) |
            (top << bot_shift) |
            (bot << top_shift);
        }
        /*
          Go for another iteration with the mirror-image of the piece.
          After both of the two iterations, we are back at the original,
          non-mirrored piece orientation.
        */
      }

      /* Now calculate a 90-degrees rotation of the piece. */
      rotated_mask = 0;
      rotated_width = piece_h;
      rotated_height = piece_w;
      for (y = 0; y < piece_h; ++y) {
        for (x = 0; x < piece_w; ++x) {
          if (mask & (1 << (W*y + x))) {
            uint32_t rotated_x = (piece_h-1) - y;
            uint32_t rotated_y = x;
            rotated_mask |= 1 << (W*rotated_y + rotated_x);
          }
        }
      }
      mask = rotated_mask;
      piece_w = rotated_width;
      piece_h = rotated_height;
    }
    avail[i].n = idx;

#ifdef VERBOSE
    printf("Piece %u: orientations=%u\n", (unsigned)i, (unsigned)avail[i].n);
#endif

  }
}


static void
found_solution(struct placement solution[NUM_PIECES])
{
  printf("Hey, found a solution!\n");
  print_solution(solution, NUM_PIECES);
}


static void
recurse(uint32_t iter, uint32_t state,
        struct piece_orientations pieces_available[NUM_PIECES],
        struct placement solution[NUM_PIECES])
{
  uint32_t i, j, x, y;

  if (iter == NUM_PIECES)
    return found_solution(solution);

  /* Iterate over the remaining pieces. */
  for (i = iter; i < NUM_PIECES; ++i) {
    struct piece_orientations p;
    uint32_t num_orientations;

    p = pieces_available[i];
    pieces_available[i] = pieces_available[0];
    num_orientations = p.n;

    /* Iterate over the possible orientations. */
    for (j = 0; j < num_orientations; ++j) {
      uint32_t mask = p.masks[j];
      uint32_t x_max = W - p.w[j];
      uint32_t y_max = H - p.h[j];
      for (y = 0; y <= y_max; ++y) {
        for (x = 0; x <= x_max; ++x) {
          uint32_t piece_mask;
          uint32_t new_state;

          piece_mask = mask << (x+W*y);
          if (piece_mask & state)
            continue;                           /* Does not fit here */
          new_state = state | piece_mask;
          solution[iter].piece_mask = piece_mask;
          recurse(iter+1, new_state, pieces_available, solution);
        }
      }
    }

    /* Restore pieces_available[] array for next iteration. */
    pieces_available[i] = p;
  }
}


int
main(int argc, char *argv[])
{
  struct piece_orientations pieces_available[NUM_PIECES];
  struct placement solution[NUM_PIECES];

  calc_pieces_available(pieces_available);
  recurse(0, 0, pieces_available, solution);

  return 0;
}
