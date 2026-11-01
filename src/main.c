#include <raymath.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>

#include "tile.h"
#include "raylib.h"

#define __TIMER_IMPLEMENTATION__
#include "timer.h"

#define NOB_IMPLEMENTATION
#include "nob.h"

#include "fheap.h"

Color color_conv[4] = {
  {.r=230, .g=41 , .b=55 , .a=255},
  {.r=0  , .g=121, .b=241, .a=255},
  {.r=0  , .g=228, .b=48 , .a=255},
  {.r=253, .g=249, .b=0  , .a=255}
};

typedef struct
{
  pos pos;
  uint8_t side;
} sided_pos;


#define ID_GRID_SIDE_LENGTH (2 * TILE_COUNT + 1)
typedef int64_t id_grid[ ID_GRID_SIDE_LENGTH * ID_GRID_SIDE_LENGTH ];

#define ID_GRID_AT(grid, x, y) ((grid)[ ID_GRID_SIDE_LENGTH * ((x) + TILE_COUNT) + ((y) + TILE_COUNT)])

#define COS_2PI_OVER_12 0.86602540378444

const double radius = 60;
const double pad = 2;
const double grid_step = 2 * radius * COS_2PI_OVER_12;
const double arc_thickness = 4.0;
const double arc_pad = 4;
const int screenWidth = 1000, screenHeight = 600;
const Rectangle textBox = {.x = screenWidth - 100, .y = 5, .width = 95, .height = 30};

#define POS_PLUS_POLAR(posx, posy, r, theta) posx + (r) * cos((theta)), posy - (r) * sin((theta))
#define POS_PLUS_POLAR_X(pos, r, theta) POS_PLUS_POLAR(pos, r, theta)
#define POS_PLUS_EGAL_POLAR(posx, posy, r, theta) POS_PLUS_POLAR(posx = posx, posy = posy, r, theta)
#define VEC2_POS_PLUS_POLAR(pos, r, theta) \
  (Vector2) { POS_PLUS_POLAR(((pos).x), ((pos).y), r, theta) }
#define VEC2_POS_PLUS_POLAR_X(pos, r, theta) VEC2_POS_PLUS_POLAR((pos), r, theta)

#define VAL_FROM_POS(pos) (*(uint64_t *)&(pos))
#define POS_FROM_VAL(val) (*(pos *)&(val))

uint8_t tile_fits(id_grid grid, tile *tiles, size_t tile_idx);
uint8_t find_solution(id_grid, tile tiles[TILE_COUNT], size_t tile_count);
uint8_t verify_solution(id_grid grid, tile tiles[TILE_COUNT], size_t tile_count);
void DrawTile(tile t);
void DrawTiles(tile tiles[TILE_COUNT], size_t tile_count);
Vector2 TileGetWorldPos(tile t);
uint64_t distance_between_ends(id_grid grid, sided_pos end1, sided_pos end2);
Vector2 GetWorldPos(pos p);

#define CAMERA_SPEED 4.0

int main(int argc, char** argv)
{
  (void) argc, (void) argv;

  uint32_t difficulty = 3;

  id_grid grid = {0};

  for (int32_t x = -TILE_COUNT; x <= TILE_COUNT; ++x)
    for (int32_t y = -TILE_COUNT; y <= TILE_COUNT; ++y)
      ID_GRID_AT(grid, x, y) = -1;

  InitWindow(screenWidth, screenHeight, "tantrix");

  SetTargetFPS(60);

  Camera2D camera = (Camera2D){0};
  camera.target = (Vector2){0, 0};
  camera.offset = (Vector2){screenWidth * 1.0 / 2, screenHeight * 1.0 / 2};
  camera.rotation = 0.0;
  camera.zoom = 1.0;

  uint8_t refind = 1;
  char text[32];
  int64_t char_count = 0;
  while(!WindowShouldClose())
  {
    if (IsKeyDown(KEY_A))
      camera.target = VEC2_POS_PLUS_POLAR(camera.target, -CAMERA_SPEED, camera.rotation * DEG2RAD);
    if (IsKeyDown(KEY_D))
      camera.target = VEC2_POS_PLUS_POLAR(camera.target, CAMERA_SPEED, camera.rotation * DEG2RAD);
    if (IsKeyDown(KEY_W))
      camera.target = VEC2_POS_PLUS_POLAR(camera.target, CAMERA_SPEED, camera.rotation * DEG2RAD + PI/2);
    if (IsKeyDown(KEY_S))
      camera.target = VEC2_POS_PLUS_POLAR(camera.target, -CAMERA_SPEED, camera.rotation * DEG2RAD + PI/2);
    if (IsKeyDown(KEY_Q)) camera.rotation += CAMERA_SPEED;
    if (IsKeyDown(KEY_E)) camera.rotation -= CAMERA_SPEED;

    camera.zoom = expf(logf(camera.zoom) + (float)GetMouseWheelMove() * 0.1f);

    if (IsKeyPressed(KEY_R))
    {
      camera.rotation = 0;
      camera.zoom = 1.0;
    }

    if (IsKeyPressed(KEY_RIGHT))
    {
      ++difficulty;
      refind = 1;
    }

    if (IsKeyPressed(KEY_LEFT))
    {
      --difficulty;
      refind = 1;
    }

    int key = GetCharPressed();

    while (key > 0)
    {
      // only accept digits
      if (key >= '0' && key <= '9' && char_count < 2)
      {
        text[char_count++] = key;
        text[char_count] = '\0';
      }

      key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE))
    {
      --char_count;
      if (char_count < 0) char_count = 0;
      text[char_count] = '\0'; // zero-terminate
    }

    if (IsKeyPressed(KEY_ENTER))
    {
      sscanf(text, "%u", &difficulty);
      refind = 1;
    }

    if (difficulty > 30 || difficulty < 3)
    {
      fprintf(stderr, "[ERROR] difficulty must be between 3 and 30 but got %u!!!\n", difficulty);
      difficulty = 30;
    }

    // preplace tile 0
    tiles[0].used = 1;
    tiles[0].x = 0;
    tiles[0].y = 0;
    tiles[0].rotation = 0;
    ID_GRID_AT(grid, 0, 0) = 0;

    BeginDrawing();
    ClearBackground(RAYWHITE);
    BeginMode2D(camera);

    if (refind)
    {
      for (int32_t x = -TILE_COUNT; x <= TILE_COUNT; ++x)
        for (int32_t y = -TILE_COUNT; y <= TILE_COUNT; ++y)
          ID_GRID_AT(grid, x, y) = -1;

      for (size_t i = 0; i < TILE_COUNT; ++i)
        tiles[i].used = 0;

      timer time = {0};
      timer_start(&time);
      printf("%u\n", find_solution(grid, tiles, difficulty));
      const double run_time_secs = timer_stop(&time);
      const uint32_t mins = run_time_secs / 60;
      const double secs = run_time_secs - 60.0 * mins;
      printf("[INFO] Took %um%.3lfs\n", mins, secs);

      const uint8_t valid = verify_solution(grid, tiles, difficulty);
      printf("is %s a solution\n", valid ? "actually" : "not");

      if (!valid)
        return 1;

      sprintf(text, "%u", difficulty);
      refind = 0;
    }

    DrawTiles(tiles, difficulty);

    EndMode2D();

    DrawRectangle(0, 0, 40, 20, color_conv[tiles[difficulty - 1].back]);
    DrawRectangleRec(textBox, GRAY);
    DrawText(text, textBox.x + 5, textBox.y + 5, 20, BLACK);
    EndDrawing();
  }

  CloseWindow();

  return 0;
}

uint8_t tile_fits(id_grid grid, tile *tiles, size_t tile_idx)
{
  for (uint8_t side = 0; side < SIDE_COUNT; ++side)
  {
    const uint8_t symetric_side = (side + 3) % SIDE_COUNT;

    const int32_t x = tiles[tile_idx].x + adj[side].x;
    const int32_t y = tiles[tile_idx].y + adj[side].y;
    int64_t neighboor = ID_GRID_AT(grid, x, y);
    if (neighboor < 0) continue; // there is no neighboor there : will always fit
    if (TILE_GET_COLOR(tiles[tile_idx], side) != TILE_GET_COLOR(tiles[neighboor], symetric_side))
      return 0;
  }

  return 1;
}

/*
 * find the side(s) with the back color
 * return negative in case of failure : if no open side had color c
 * The only case where a tile might have two of its sides back colored and open is the first tile
 * we only need to find the first instance as the other will be checked later when the first one gets used by other piece
 */
int8_t find_colored_side(id_grid grid, tile tiles[TILE_COUNT], size_t tile_idx, color c)
{
    for (uint8_t side = 0; side < SIDE_COUNT; ++side)
    {
      if (TILE_GET_COLOR(tiles[tile_idx], side) != c)
        continue;
      const int32_t x = tiles[tile_idx].x + adj[side].x;
      const int32_t y = tiles[tile_idx].y + adj[side].y;
      int64_t neighboor = ID_GRID_AT(grid, x, y);
      if (neighboor < 0)
        return side;
    }

    return -1;
}

double progress_per_tile(size_t tile_count, size_t placed_count)
{
  double acc = 1;

  for (size_t i = 1; i <= placed_count; ++i)
    acc *= 1.0 / (tile_count - i);

  return acc;
}

uint64_t distance_between_ends_dijsktra(id_grid grid, sided_pos end1, sided_pos end2)
{
  const int64_t INF = INT64_MAX;

  const pos start = {.x = end1.pos.x + adj[end1.side].x,
                     .y = end1.pos.y + adj[end1.side].y};
  const pos end = {.x = end2.pos.x + adj[end2.side].x,
                   .y = end2.pos.y + adj[end2.side].y};

  id_grid d = {0};
  for (int32_t x = -TILE_COUNT; x <= TILE_COUNT; ++x)
    for (int32_t y = -TILE_COUNT; y <= TILE_COUNT; ++y)
      ID_GRID_AT(d, x, y) = INF;

  ID_GRID_AT(d, start.x, start.y) = 0;

  id_grid visited = {0};
  for (int32_t x = -TILE_COUNT; x <= TILE_COUNT; ++x)
    for (int32_t y = -TILE_COUNT; y <= TILE_COUNT; ++y)
      ID_GRID_AT(visited, x, y) = 0;

  fheap file = {0};
  fheap_init(&file);

  fheap_insert(&file, 0, VAL_FROM_POS(start));

  key_val_pair node;
  while (file.n > 0)
  {
    node = fheap_extract_min(&file);
    const pos curr = POS_FROM_VAL(node.val);
    const double dx = node.key;

    if (node.val == VAL_FROM_POS(end))
      break;

    if (ID_GRID_AT(visited, curr.x, curr.y)) continue;

    for (uint8_t side = 0; side < SIDE_COUNT; ++side)
    {
      const pos neighbour = {.x = curr.x + adj[side].x, .y = curr.y + adj[side].y};

      if (ID_GRID_AT(grid, neighbour.x, neighbour.y) >= 0) // ther is already a tile there dont try to go there
        continue;

      if (dx + 1 < ID_GRID_AT(d, neighbour.x, neighbour.y)) // + 1 because distance between x and y is one
      {
        ID_GRID_AT(d, neighbour.x, neighbour.y) = dx + 1;
        fheap_insert(&file, ID_GRID_AT(d, neighbour.x, neighbour.y), VAL_FROM_POS(neighbour));
      }
    }

    ID_GRID_AT(visited, curr.x, curr.y) = 1;
  }

  fheap_clear(&file);

  return (uint64_t) ID_GRID_AT(d, end.x, end.y);
}

uint64_t distance_between_ends(id_grid grid, sided_pos end1, sided_pos end2)
{
  // return distance_between_ends_astar(grid, tiles, tile_count, end1, end2);
  return distance_between_ends_dijsktra(grid, end1, end2);
}

uint8_t find_solution_inside(id_grid grid, tile tiles[TILE_COUNT], size_t tile_count, int64_t placed_tiles[TILE_COUNT], size_t placed_count, double progress, sided_pos end1, sided_pos end2)
{
  printf("\r%.2lf%%", 100.0 * progress);


  if (placed_count == tile_count)
    return verify_solution(grid, tiles, tile_count);

  /*
   * Check if we have enough tiles to go from one end point to the other
   * In the case we dont: this branch is doomed there is not point continuing the search here
   */
  if (distance_between_ends(grid, end1, end2) > tile_count - placed_count)
    return 0;

  const int32_t x = end1.pos.x + adj[end1.side].x;
  const int32_t y = end1.pos.y + adj[end1.side].y;

  /*
   * There is already a tile at the end of end1 ie we have looped, but too early since we have placed less than tile_count tiles
   * This branch is hence a failure: return 0
   */
  if (ID_GRID_AT(grid, x, y) >= 0)
    return 0;

  const color back = tiles[tile_count - 1].back;

  for (size_t tile = 1; tile < tile_count; ++tile)
  {
    if (tiles[tile].used) continue;

    tiles[tile].x = x;
    tiles[tile].y = y;
    tiles[tile].used = 1;
    placed_tiles[placed_count] = tile;
    ID_GRID_AT(grid, x, y) = tile;

    if (tiles[tile].shape[back][0] < 0 && tiles[tile].shape[back][1] < 0)
    {
      fprintf(stderr, "[ERROR] back color %u not found on tile %zu!!\n", back, tile);
      exit(-1);
    }

    /*
     * we want to aligne the color at rot with (side + 3) % SIDE_COUNT
     * We hence need to rotate by (side + 3) % SIDE_COUNT - rot all mode SIDE_COUNT
     *
     * We need to do that with both the orientations
     */
    for (uint8_t i = 0; i < 2; ++i)
    {
      tiles[tile].rotation = (end1.side + 3 - tiles[tile].shape[back][i] + SIDE_COUNT) % SIDE_COUNT;
      if (!tile_fits(grid, tiles, tile)) continue;

      if (
          find_solution_inside(grid, tiles, tile_count,
            placed_tiles, placed_count + 1,
            progress,
            (sided_pos){.pos = {x, y},
                              .side = (tiles[tile].shape[back][1 - i] + tiles[tile].rotation) % SIDE_COUNT},
            end2)
          )
        return 1; // we found a solution
    }

    // reset all the tiles placed after
    for (size_t j = placed_count; j < tile_count; ++j)
    {
      if (placed_tiles[j] < 0) continue;

      ID_GRID_AT(grid,
          tiles[placed_tiles[j]].x,
          tiles[placed_tiles[j]].y) = -1;
      tiles[placed_tiles[j]].used = 0;
      placed_tiles[j] = -1;
    }

    // update the progress
    progress += progress_per_tile(tile_count, placed_count);
  }

  return 0;
}

/*
 * tries to place all of the tiles in `tiles`
 * return non-zero uppon success
 */
uint8_t find_solution(id_grid grid, tile tiles[TILE_COUNT], size_t tile_count)
{
  int64_t placed_tiles[TILE_COUNT] = {0};

  for (size_t i = 0; i < TILE_COUNT; ++i)
    placed_tiles[i] = -1;

  // preplace tile 0
  tiles[0].used = 1;
  tiles[0].x = 0;
  tiles[0].y = 0;
  tiles[0].rotation = 0;
  placed_tiles[0] = 0;
  ID_GRID_AT(grid, 0, 0) = 0;

  const color back = tiles[tile_count - 1].back;

  if (tiles[0].shape[back][0] < 0 && tiles[0].shape[back][1] < 0)
  {
    fprintf(stderr, "[ERROR] back color %u not found on tile 0!!\n", back);
    exit(-1);
  }

  return find_solution_inside(grid, tiles, tile_count, placed_tiles, 1, 0,
      (sided_pos){.pos = {0, 0}, .side = tiles[0].shape[back][0]},
      (sided_pos){.pos = {0, 0}, .side = tiles[0].shape[back][1]});
}

#define VERIFY_LOOP_START_TILE 0

uint8_t verify_solution(id_grid grid, tile tiles[TILE_COUNT], size_t tile_count)
{
  // sanity check
  for (size_t i = 0; i < tile_count; ++i)
    if (!tile_fits(grid, tiles, i)) return 0;

  // continuous check
  color back = tiles[tile_count - 1].back;
  uint8_t seen[TILE_COUNT] = {0};
  memset(seen, 0, sizeof(seen));

  int64_t curr = VERIFY_LOOP_START_TILE;
  seen[curr] = 1;

  uint8_t side = tiles[curr].shape[back][0]
    , arrived_from;

  curr = ID_GRID_AT(grid,
      tiles[curr].x + adj[side].x,
      tiles[curr].y + adj[side].y
      );
  if (curr < 0) return 0;
  arrived_from = (side + 3) % SIDE_COUNT;
  seen[curr] = 1;

  while (curr != VERIFY_LOOP_START_TILE)
  {
    side = (tiles[curr].shape[back][0] + tiles[curr].rotation) % SIDE_COUNT;
    if (side == arrived_from)
      side = (tiles[curr].shape[back][1] + tiles[curr].rotation) % SIDE_COUNT;

    curr = ID_GRID_AT(grid,
        tiles[curr].x + adj[side].x,
        tiles[curr].y + adj[side].y
        );
    if (curr < 0) return 0;
    arrived_from = (side + 3) % SIDE_COUNT;
    seen[curr] = 1;
  }

  for (size_t i = 0; i < tile_count; ++i)
    if (!seen[i]) return 0;

  return 1;
}

void DrawPaddedRing(Vector2 center, double innerRadius, double outerRadius, double startAngle, double endAngle, double pad, int segments, Color ringColor, Color padColor)
{
  DrawRing(center, innerRadius - pad, outerRadius + pad, startAngle, endAngle, segments, padColor);
  DrawRing(center, innerRadius, outerRadius, startAngle, endAngle, segments, ringColor); // main ring

  return;
}

void DrawPaddedLineEx(Vector2 startPos, Vector2 endPos, double thick, double pad, Color color, Color padColor)
{
  DrawLineEx(startPos, endPos, thick + 2 * pad, padColor);
  DrawLineEx(startPos, endPos, thick, color); // main line

  return;
}

void DrawTile(tile t)
{
  const Vector2 center = TileGetWorldPos(t);
  DrawPoly(center, SIDE_COUNT, radius, 30, BLACK);

  // DrawCircleV(center, 5, PURPLE);

  for (uint8_t col = 0; col < COLOR_COUNT; ++col)
  {
    if (t.shape[col][0] < 0 && t.shape[col][1] < 0) continue; // color not found
    const int8_t side1 = (t.shape[col][0] + t.rotation) % SIDE_COUNT;
    const int8_t side2 = (t.shape[col][1] + t.rotation) % SIDE_COUNT;

    const uint8_t delta = (side2 + SIDE_COUNT - side1) % SIDE_COUNT;
    double angle = 90;
    switch (delta) {
      case 5:
        angle += 5 * 60;
        // fallthrough
      case 1:
        // smol one
        angle -= side2 * 60;

        const Vector2 smol_center = VEC2_POS_PLUS_POLAR(center, radius, angle * DEG2RAD);

        // DrawCircleV(arc_center, 5, MAGENTA);

        DrawPaddedRing(smol_center,
            radius / 2 - arc_thickness,
            radius / 2 + arc_thickness,
            -angle + 120,
            -angle + 240,
            arc_pad,
            100,
            color_conv[col],
            BLACK);
       break;

      case 4:
       angle += 30 - side1 * 60;
       angle += -30 + side2 * 60; // compensate for the fallthrough
       // fallthrough
      case 2:
       angle -= -30 + side2 * 60;
       // big one

       const Vector2 big_center = VEC2_POS_PLUS_POLAR(center, grid_step, angle * DEG2RAD);

       DrawPaddedRing(big_center,
           3.0 / 2.0 * radius - arc_thickness,
           3.0 / 2.0 * radius + arc_thickness,
           -angle - 210, -angle - 150,
           arc_pad,
           100, color_conv[col], BLACK);
       break;

      case 3:
       // line
       angle = 60 - side1 * 60;

       DrawPaddedLineEx(
           VEC2_POS_PLUS_POLAR(center, grid_step / 2, angle * DEG2RAD),
           VEC2_POS_PLUS_POLAR(center, grid_step / 2, (angle + 180) * DEG2RAD),
           2 * arc_thickness, arc_pad,
           color_conv[col], BLACK);
       break;

     default:
       fprintf(stderr, "[UNREACHABLE] delta did not match any know value, got: %u!!\n", delta);
       return;
      }
  }

  return;
}

void DrawTiles(tile tiles[TILE_COUNT], size_t tile_count)
{
  for (size_t i = 0; i < tile_count; ++i)
    if (tiles[i].used)
      DrawTile(tiles[i]);

  return;
}

Vector2 TileGetWorldPos(tile t)
{
  return GetWorldPos((pos){t.x, t.y});
}

Vector2 GetWorldPos(pos p)
{
  const double radius = 60;
  const double grid_step = 2 * radius * cos (2 * PI / 12);

  return (Vector2){
    .x=p.x * (grid_step + pad)
      + p.y * (grid_step + pad) * cos(2 * PI / 6),
    .y=-p.y * (grid_step + pad) * sin(2 * PI / 6)
  };
}
