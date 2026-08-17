/*
 * hello.c
 *
 * Simple elf2x68k / Human68k sample.
 *
 * Usage:
 *   hello.x model.obj
 *
 * Loads a Wavefront OBJ file and draws its wireframe
 * in the center of the X68000 graphics screen.
 */

#include <x68k/iocs.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define SCREEN_W 256
#define SCREEN_H 256
#define SCREEN_MARGIN 20

#define INITIAL_VERTEX_CAPACITY 256
#define INITIAL_EDGE_CAPACITY   512
#define MAX_FACE_VERTICES        64


typedef struct {
  float x;
  float y;
  float z;
} Vertex;


typedef struct {
  int a;
  int b;
} Edge;


typedef struct {
  Vertex *vertices;
  int vertex_count;
  int vertex_capacity;

  Edge *edges;
  int edge_count;
  int edge_capacity;
} Mesh;


/*
 * Add a vertex to the mesh.
 */
static int mesh_add_vertex(Mesh *mesh, float x, float y, float z)
{
  Vertex *new_vertices;
  int new_capacity;

  if (mesh->vertex_count >= mesh->vertex_capacity) {
    if (mesh->vertex_capacity == 0) {
      new_capacity = INITIAL_VERTEX_CAPACITY;
    } else {
      new_capacity = mesh->vertex_capacity * 2;
    }

    new_vertices = (Vertex *)realloc(
      mesh->vertices,
      (size_t)new_capacity * sizeof(Vertex));

    if (new_vertices == NULL) {
      return 0;
    }

    mesh->vertices = new_vertices;
    mesh->vertex_capacity = new_capacity;
  }

  mesh->vertices[mesh->vertex_count].x = x;
  mesh->vertices[mesh->vertex_count].y = y;
  mesh->vertices[mesh->vertex_count].z = z;
  mesh->vertex_count++;

  return 1;
}


/*
 * Add one wireframe edge.
 */
static int mesh_add_edge(Mesh *mesh, int a, int b)
{
  Edge *new_edges;
  int new_capacity;

  if (a == b) {
    return 1;
  }

  if (mesh->edge_count >= mesh->edge_capacity) {
    if (mesh->edge_capacity == 0) {
      new_capacity = INITIAL_EDGE_CAPACITY;
    } else {
      new_capacity = mesh->edge_capacity * 2;
    }

    new_edges = (Edge *)realloc(
      mesh->edges,
      (size_t)new_capacity * sizeof(Edge));

    if (new_edges == NULL) {
      return 0;
    }

    mesh->edges = new_edges;
    mesh->edge_capacity = new_capacity;
  }

  mesh->edges[mesh->edge_count].a = a;
  mesh->edges[mesh->edge_count].b = b;
  mesh->edge_count++;

  return 1;
}


/*
 * Convert an OBJ vertex index into a zero-based index.
 *
 * Supported forms include:
 *
 *   1
 *   1/2
 *   1/2/3
 *   1//3
 *   -1
 *
 * Negative OBJ indices are relative to the current end
 * of the vertex array.
 */
static int parse_vertex_index(const char *s, int vertex_count)
{
  char *end;
  long index;

  index = strtol(s, &end, 10);

  if (end == s || index == 0) {
    return -1;
  }

  if (index > 0) {
    index--;
  } else {
    index = vertex_count + index;
  }

  if (index < 0 || index >= vertex_count) {
    return -1;
  }

  return (int)index;
}


/*
 * Load positions and polygon edges from a Wavefront OBJ file.
 *
 * Only "v" and "f" records are required.
 * Texture coordinates, normals, materials, etc. are ignored.
 *
 * Faces are kept as polygon boundaries instead of being triangulated,
 * so a quad does not get an unwanted diagonal in wireframe mode.
 */
static int load_obj(const char *filename, Mesh *mesh)
{
  FILE *fp;
  char line[1024];

  fp = fopen(filename, "rb");

  if (fp == NULL) {
    return 0;
  }

  while (fgets(line, sizeof(line), fp) != NULL) {
    char *p;

    p = line;

    while (*p != '\0' && isspace((unsigned char)*p)) {
      p++;
    }

    if (*p == '\0' || *p == '#') {
      continue;
    }

    /*
     * Vertex:
     *
     *   v x y z
     */
    if (p[0] == 'v' && isspace((unsigned char)p[1])) {
      float x;
      float y;
      float z;

      if (sscanf(p + 1, "%f %f %f", &x, &y, &z) == 3) {
        if (!mesh_add_vertex(mesh, x, y, z)) {
          fclose(fp);
          return 0;
        }
      }
    }

    /*
     * Face:
     *
     *   f 1 2 3
     *   f 1/1 2/2 3/3
     *   f 1/1/1 2/2/2 3/3/3
     *   f 1//1 2//2 3//3
     */
    else if (p[0] == 'f' && isspace((unsigned char)p[1])) {
      int indices[MAX_FACE_VERTICES];
      int count;
      char *q;

      count = 0;
      q = p + 1;

      while (*q != '\0') {
        int index;

        while (*q != '\0' &&
             isspace((unsigned char)*q)) {
          q++;
        }

        if (*q == '\0' ||
          *q == '\r' ||
          *q == '\n' ||
          *q == '#') {
          break;
        }

        if (count >= MAX_FACE_VERTICES) {
          break;
        }

        index = parse_vertex_index(q, mesh->vertex_count);

        if (index >= 0) {
          indices[count++] = index;
        }

        while (*q != '\0' &&
             !isspace((unsigned char)*q)) {
          q++;
        }
      }

      /*
       * Store only polygon boundary edges.
       */
      if (count >= 2) {
        int i;

        for (i = 0; i < count; i++) {
          int a;
          int b;

          a = indices[i];
          b = indices[(i + 1) % count];

          if (!mesh_add_edge(mesh, a, b)) {
            fclose(fp);
            return 0;
          }
        }
      }
    }
  }

  fclose(fp);

  return mesh->vertex_count > 0 &&
       mesh->edge_count > 0;
}


/*
 * Simple fixed 3D -> 2D projection.
 *
 * This is roughly an isometric projection and intentionally avoids
 * sin(), cos(), matrices, or any OpenGL dependency.
 */
static void project_vertex(
  const Vertex *v,
  float cx,
  float cy,
  float cz,
  float *x,
  float *y)
{
  float px;
  float py;
  float pz;

  px = v->x - cx;
  py = v->y - cy;
  pz = v->z - cz;

  /*
   * cos(30 deg) ~= 0.866
   *
   * The constants give a simple fixed isometric-like view.
   */
  *x = 0.8660254f * (px - pz);
  *y = py + 0.5f * (px + pz);
}


/*
 * Convert a projected point into X68000 screen coordinates.
 */
static void vertex_to_screen(
  const Vertex *v,
  float cx,
  float cy,
  float cz,
  float projected_cx,
  float projected_cy,
  float scale,
  int *screen_x,
  int *screen_y)
{
  float x;
  float y;

  project_vertex(v, cx, cy, cz, &x, &y);

  x = (x - projected_cx) * scale;
  y = (y - projected_cy) * scale;

  *screen_x = (int)(SCREEN_W * 0.5f + x);
  *screen_y = (int)(SCREEN_H * 0.5f - y);
}


/*
 * Draw the complete mesh with IOCS LINE.
 */
static void draw_mesh(const Mesh *mesh)
{
  float min_x;
  float min_y;
  float min_z;
  float max_x;
  float max_y;
  float max_z;

  float center_x;
  float center_y;
  float center_z;

  float projected_min_x;
  float projected_min_y;
  float projected_max_x;
  float projected_max_y;

  float projected_center_x;
  float projected_center_y;

  float width;
  float height;
  float scale_x;
  float scale_y;
  float scale;

  int i;

  static struct iocs_lineptr param = {
    0, 0,
    0, 0,
    0xffff,
    0xffff
  };


  /*
   * Calculate the 3D bounding box.
   */
  min_x = max_x = mesh->vertices[0].x;
  min_y = max_y = mesh->vertices[0].y;
  min_z = max_z = mesh->vertices[0].z;

  for (i = 1; i < mesh->vertex_count; i++) {
    const Vertex *v;

    v = &mesh->vertices[i];

    if (v->x < min_x) min_x = v->x;
    if (v->x > max_x) max_x = v->x;

    if (v->y < min_y) min_y = v->y;
    if (v->y > max_y) max_y = v->y;

    if (v->z < min_z) min_z = v->z;
    if (v->z > max_z) max_z = v->z;
  }

  center_x = (min_x + max_x) * 0.5f;
  center_y = (min_y + max_y) * 0.5f;
  center_z = (min_z + max_z) * 0.5f;


  /*
   * Calculate the bounding box after projection.
   */
  project_vertex(
    &mesh->vertices[0],
    center_x,
    center_y,
    center_z,
    &projected_min_x,
    &projected_min_y);

  projected_max_x = projected_min_x;
  projected_max_y = projected_min_y;

  for (i = 1; i < mesh->vertex_count; i++) {
    float x;
    float y;

    project_vertex(
      &mesh->vertices[i],
      center_x,
      center_y,
      center_z,
      &x,
      &y);

    if (x < projected_min_x) projected_min_x = x;
    if (x > projected_max_x) projected_max_x = x;

    if (y < projected_min_y) projected_min_y = y;
    if (y > projected_max_y) projected_max_y = y;
  }

  projected_center_x =
    (projected_min_x + projected_max_x) * 0.5f;

  projected_center_y =
    (projected_min_y + projected_max_y) * 0.5f;

  width = projected_max_x - projected_min_x;
  height = projected_max_y - projected_min_y;

  if (width < 0.0001f) {
    width = 1.0f;
  }

  if (height < 0.0001f) {
    height = 1.0f;
  }

  scale_x =
    (float)(SCREEN_W - SCREEN_MARGIN * 2) / width;

  scale_y =
    (float)(SCREEN_H - SCREEN_MARGIN * 2) / height;

  scale = scale_x;

  if (scale_y < scale) {
    scale = scale_y;
  }


  /*
   * Draw all polygon edges.
   */
  for (i = 0; i < mesh->edge_count; i++) {
    const Edge *edge;
    const Vertex *a;
    const Vertex *b;

    int x1;
    int y1;
    int x2;
    int y2;

    edge = &mesh->edges[i];

    a = &mesh->vertices[edge->a];
    b = &mesh->vertices[edge->b];

    vertex_to_screen(
      a,
      center_x,
      center_y,
      center_z,
      projected_center_x,
      projected_center_y,
      scale,
      &x1,
      &y1);

    vertex_to_screen(
      b,
      center_x,
      center_y,
      center_z,
      projected_center_x,
      projected_center_y,
      scale,
      &x2,
      &y2);

    param.x1 = x1;
    param.y1 = y1;
    param.x2 = x2;
    param.y2 = y2;

    _iocs_line(&param);
  }
}


static void mesh_free(Mesh *mesh)
{
  free(mesh->vertices);
  free(mesh->edges);

  memset(mesh, 0, sizeof(*mesh));
}


int main(int argc, char *argv[])
{
  Mesh mesh;
  memset(&mesh, 0, sizeof(mesh));

  _iocs_crtmod(0x0e);
  _iocs_g_clr_on();

  printf("Hello! World\n");

  if (argc != 2) {
    printf("Usage: hello.x model.obj\n");
    return 1;
  }

  if (!load_obj(argv[1], &mesh)) {
    printf("Cannot load OBJ: %s\n", argv[1]);
    mesh_free(&mesh);
    return 1;
  }

  draw_mesh(&mesh);
  mesh_free(&mesh);

  return 0;
}