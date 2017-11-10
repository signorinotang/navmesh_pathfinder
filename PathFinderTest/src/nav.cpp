#include "nav.h"


#define max(a,b)    (((a) > (b)) ? (a) : (b))

#define min(a,b)    (((a) < (b)) ? (a) : (b))

#define cross(vt1,vt2) ((vt1)->z * (vt2)->x - (vt1)->x * (vt2)->z)

#define cross_point(a,b,c,d,result) do {result->x = ((b->x - a->x) * (c->x - d->x) * (c->z - a->z) - c->x * (b->x - a->x) * (c->z - d->z) + a->x * (b->z - a->z) * (c->x - d->x))/((b->z - a->z)*(c->x - d->x) - (b->x - a->x) * (c->z - d->z));result->z = ((b->z - a->z) * (c->z - d->z) * (c->x - a->x) - c->z * (b->z - a->z) * (c->x - d->x) + a->z * (b->x - a->x) * (c->z - d->z))/((b->x - a->x)*(c->z - d->z) - (b->z - a->z) * (c->x - d->x));} while (0)

#define vector3_copy(dst,src) do {(dst)->x = (src)->x;(dst)->y = (src)->y;(dst)->z = (src)->z;} while(0)

#define vector3_sub(a,b,result) do {(result)->x = (a)->x - (b)->x;(result)->y = (a)->y - (b)->y;(result)->z = (a)->z - (b)->z;} while(0)

#define cast_node(elt) ((struct nav_node*)((int8_t*)elt - sizeof(struct list_node)))

void set_mask(struct nav_mesh_mask* ctx,int mask,int enable)
{
	if (mask >= ctx->size)
	{
		int nsize = ctx->size * 2;
		int* mask_ptr = ctx->mask;
		ctx->mask = (int*)malloc(sizeof(int) * nsize);
		memcpy(ctx->mask, mask_ptr, sizeof(int) * ctx->size);
		ctx->size = nsize;
		free(mask_ptr);
	}
	ctx->mask[mask] = enable;
}

bool intersect(struct vector3* a,struct vector3* b,struct vector3* c,struct vector3* d)
{
	//¿ìËÙÅÅ³âÊµÑé,¾ØÐÎÏà½»
	if (max(a->x,b->x) >= min(c->x,d->x) &&
		max(a->z,b->z) >= min(c->z,d->z) && 
		max(c->x,d->x) >= min(a->x,b->x) && 
		max(c->z,d->z) >= min(a->z,b->z))
	{
		struct vector3 ac,dc,bc,ca,ba,da;
		vector3_sub(a,c,&ac);
		vector3_sub(d,c,&dc);
		vector3_sub(b,c,&bc);
		vector3_sub(c,a,&ca);
		vector3_sub(b,a,&ba);
		vector3_sub(d,a,&da);

		if (cross(&ac,&dc) * cross(&dc,&bc) >= 0)
		{
			if (cross(&ca,&ba) * cross(&ba,&da) >= 0)
				return true;
		}
	}
	
	return false;
}

bool inside_poly(struct nav_mesh_context* mesh_ctx, int* poly, int size, struct vector3* vt3)
{
	int forward = 0;
	for (int i = 0;i < size;i++)
	{
		struct vector3* vt1 = &mesh_ctx->vertices[poly[i]];
		struct vector3* vt2 = &mesh_ctx->vertices[poly[(i+1)%size]];

		struct vector3 vt21;
		vt21.x = vt2->x - vt1->x;
		vt21.y = 0;
		vt21.z = vt2->z - vt1->z;

		struct vector3 vt31;
		vt31.x = vt3->x - vt1->x;
		vt31.y = 0;
		vt31.z = vt3->z - vt1->z;

		double y = cross(&vt21,&vt31);
		if (y == 0)
			continue;

		if (forward == 0)
			forward = y > 0? 1:-1;
		else
		{
			if (forward == 1 && y < 0)
				return false;
			else if (forward == -1 && y > 0)
				return false;
		}
	}
	return true;
}

bool inside_node(struct nav_mesh_context* mesh_ctx,int polyId,double x,double y,double z)
{
	struct nav_node* nav_node = &mesh_ctx->node[polyId];
	struct vector3 vt;
	vt.x = x;
	vt.y = y;
	vt.z = z;
	return inside_poly(mesh_ctx,nav_node->poly,nav_node->size,&vt);
}

struct nav_node* get_node_with_pos(struct nav_mesh_context* ctx,double x,double y,double z)
{
	if (x < ctx->lt.x || x > ctx->br.x)
		return NULL;
	if (z < ctx->lt.z || z > ctx->br.z)
		return NULL;

#ifndef USE_NAV_TILE
	//±éÀú²éÕÒ
	for (int i = 0; i < ctx->size;i++)
	{
		if (in_node(ctx,i,x,y,z))
			return &ctx->node[i];
	}
	return NULL;
#else

	//ÀûÓÃ¸ñ×Ó¿ìËÙ²éÕÒ
	int x_index = x - ctx->lt.x;
	int z_index = z - ctx->lt.z;
	int index = x_index + z_index * ctx->width;
	struct nav_tile* tile = &ctx->tile[index];
	for (int i = 0;i < tile->offset;i++)
	{
		if (inside_node(ctx, tile->node[i], x, y, z))
			return &ctx->node[tile->node[i]];
	}
	return NULL;
#endif
}

struct nav_border* add_border(struct nav_mesh_context* mesh_ctx, int a, int b)
{
	struct nav_border_context * border_ctx = &mesh_ctx->border_ctx;
	if (border_ctx->border_offset + 1 >= border_ctx->border_cap)
	{
		int ncap = border_ctx->border_cap * 2;
		struct nav_border* oborders = border_ctx->borders;
		border_ctx->borders = (struct nav_border*)malloc(sizeof(struct nav_border) * ncap);
		memcpy(border_ctx->borders, oborders, sizeof(struct nav_border) * border_ctx->border_cap);
		border_ctx->border_cap = ncap;
		free(oborders);
	}

	struct nav_border * border = &border_ctx->borders[border_ctx->border_offset];
	border->id = border_ctx->border_offset;
	border->a = a;
	border->b = b;
	border->node[0] = -1;
	border->node[1] = -1;
	border->opposite = -1;
	border->center.x = (mesh_ctx->vertices[a].x + mesh_ctx->vertices[b].x);
	border->center.y = (mesh_ctx->vertices[a].y + mesh_ctx->vertices[b].y);
	border->center.z = (mesh_ctx->vertices[a].z + mesh_ctx->vertices[b].z);

	border_ctx->border_offset++;

	return border;
}

struct nav_border* search_border(struct nav_mesh_context* ctx,int begin,int end)
{
	if (begin >= ctx->len || end >= ctx->len)
		return NULL;

	struct nav_border_search_node* node = ctx->border_searcher[begin];
	while(node != NULL)
	{
		if (node->index == end)
			return get_border(ctx,node->id);
		node = node->next;
	}

	node =  (struct nav_border_search_node*)malloc(sizeof(*node));
	struct nav_border* border = add_border(ctx,begin,end);
	node->id = border->id;
	node->index = end;
	node->next = ctx->border_searcher[begin];
	ctx->border_searcher[begin] = node;
	return border;
}

void release_border_searcher(struct nav_mesh_context* ctx)
{
	int i;
	for(i = 0;i < ctx->len;i++)
	{
		struct nav_border_search_node* node = ctx->border_searcher[i];
		while(node != NULL)
		{
			struct nav_border_search_node* tmp = node;
			node = node->next;
			free(tmp);
		}
	}
	free(ctx->border_searcher);
}

void border_link_node(struct nav_border* border,int id)
{
	if (border->node[0] == -1)
		border->node[0] = id;
	else if (border->node[1] == -1)
		border->node[1] = id;
	else
		assert(0);
}

struct list* get_link(struct nav_mesh_context* mesh_ctx, struct nav_node* node)
{
	for (int i = 0; i < node->size;i++)
	{
		int border_index = node->border[i];
		struct nav_border* border = get_border(mesh_ctx, border_index);

		int linked = -1;
		if (border->node[0] == node->id)
			linked = border->node[1];
		else
			linked = border->node[0];

		if (linked == -1)
			continue;
		
		struct nav_node* tmp = get_node(mesh_ctx,linked);
		if (tmp->list_head.pre || tmp->list_head.next)
			continue;

		if (get_mask(mesh_ctx->mask_ctx,tmp->mask))
		{
			list_push((&mesh_ctx->linked),((struct list_node*)tmp));
			tmp->reserve = border->opposite;
			vector3_copy(&tmp->pos, &border->center);
		}
	}

	if (list_empty((&mesh_ctx->linked)))
		return NULL;
	
	return &mesh_ctx->linked;
}

double G_COST(struct nav_node* from,struct nav_node* to)
{
	double dx = from->pos.x - to->pos.x;
	//double dy = from->center.y - to->center.y;
	double dy = 0;
	double dz = from->pos.z - to->pos.z;
	return sqrt(dx*dx + dy* dy + dz* dz) * GRATE;
}

double H_COST(struct nav_node* from, struct vector3* to)
{
	double dx = from->center.x - to->x;
	//double dy = from->center.y - to->y;
	double dy = 0;
	double dz = from->center.z - to->z;
	return sqrt(dx*dx + dy* dy + dz* dz) * HRATE;
}

int node_cmp(struct element * left, struct element * right) 
{
	struct nav_node *l = cast_node(left);
	struct nav_node *r = cast_node(right);
	return l->F < r->F;
}

//¶¥µãË³Ê±ÕëÅÅÐòº¯Êý
int vertex_cmp(const void * left,const void * right) 
{
	struct vertex_sort_info *l = (struct vertex_sort_info*)left;
	struct vertex_sort_info *r = (struct vertex_sort_info*)right;
	
	struct vector3 pt0,pt1;
	vector3_copy(&pt0,&l->ctx->vertices[l->index]);
	vector3_copy(&pt1,&l->ctx->vertices[r->index]);

	if (pt0.x >= 0 && pt1.x < 0)
		return 1;
	if (pt0.x == 0 && pt1.x == 0)
		return pt0.z > pt1.z;

	struct vector3 vt0,vt1;
	vector3_sub(&pt0,&l->center,&vt0);
	vector3_sub(&pt1,&r->center,&vt1);

	double det = cross(&vt0,&vt1);
	if (det < 0)
		return 0;
	if (det > 0)
		return 1;

	return (vt0.x* vt0.x +vt0.z * vt0.z) > (vt1.x* vt1.x +vt1.z * vt1.z);
}

//Ã¿¸ö¶à±ßÐÎµÄ¶¥µãË³Ê±ÕëÅÅÐò
void vertex_sort(struct nav_mesh_context* ctx,struct nav_node* node)
{
	struct vertex_sort_info* vertex = (struct vertex_sort_info*)malloc(sizeof(*vertex) * node->size);
	for (int i = 0;i < node->size;i++)
	{
		vertex[i].ctx = ctx;
		vertex[i].index = node->poly[i];
		vector3_copy(&vertex[i].center,&node->center);
	}

	qsort(vertex,node->size,sizeof(struct vertex_sort_info),vertex_cmp);

	for (int i = 0;i < node->size;i++)
		node->poly[i] = vertex[i].index;

	free(vertex);
}

void tile_add_node(struct nav_tile* tile,int index)
{
	if (tile->size == 0)
	{
		tile->offset = 0;
		tile->size = 1;
		tile->node = (int*)malloc(sizeof(int) * tile->size);
	}
	if (tile->offset >= tile->size)
	{
		int nsize = tile->size * 2;
		int* onode = tile->node;
		tile->node = (int*)malloc(sizeof(int) * nsize);
		memcpy(tile->node,onode,sizeof(int)* tile->size);
		tile->size = nsize;
		free(onode);
	}
	tile->node[tile->offset] = index;
	tile->offset++;
}

//ÇÐ·Ö¸ñ×ÓÐÅÏ¢,Ã¿¸ö¸ñ×Ó¿çÁ¢ÁË¶àÉÙ¸ö¶à±ßÐÎ
void create_tile(struct nav_mesh_context* ctx)
{
	int count = ctx->width * ctx->heigh;
	ctx->tile = (struct nav_tile*)malloc(sizeof(struct nav_tile)*count);
	memset(ctx->tile,0,sizeof(struct nav_tile)*count);

	for (int z = 0;z < ctx->heigh;z++)
	{
		for (int x = 0;x < ctx->width;x++)
		{
			int index = x + z * ctx->width;
			struct nav_tile* tile = &ctx->tile[index];
			tile->pos[0].x = ctx->lt.x + x;
			tile->pos[0].z = ctx->lt.z + z;
			tile->pos[1].x = ctx->lt.x + x+1;
			tile->pos[1].z = ctx->lt.z + z;
			tile->pos[2].x = ctx->lt.x + x+1;
			tile->pos[2].z = ctx->lt.z + z+1;
			tile->pos[3].x = ctx->lt.x + x;
			tile->pos[3].z = ctx->lt.z + z+1;
			tile->center.x = ctx->lt.x + x + 0.5;
			tile->center.z = ctx->lt.z + z + 0.5;
		}
	}

	for (int i = 0;i < count;i++)
	{
		struct nav_tile* tile = &ctx->tile[i];
		
		for (int j = 0;j < ctx->size;j++)
		{
			bool done = false;
			struct nav_node* node = &ctx->node[j];
			for (int k = 0;k < 4;k++)
			{
				for (int l = 0;l < node->size;l++)
				{
					struct nav_border* border = get_border(ctx,node->border[l]);
					if (intersect(&tile->pos[k], &tile->pos[(k + 1) % 4], &ctx->vertices[border->a], &ctx->vertices[border->b]))
					{
						done = true;
						break;
					}
				}
				if (done)
					break;
			}
			
			if (done)
				tile_add_node(tile,j);
			else
			{
				if (inside_node(ctx, j, tile->center.x, tile->center.y, tile->center.z))
					tile_add_node(tile,j);
			}
		}
	}
}


void release_tile(struct nav_mesh_context* ctx)
{
	int count = ctx->width * ctx->heigh;
	for (int i = 0;i < count;i++)
	{
		if (ctx->tile[i].node != NULL)
			free(ctx->tile[i].node);
	}
	free(ctx->tile);
}

struct nav_mesh_context* load_mesh(double** v,int v_cnt,int** p,int p_cnt)
{
	struct nav_mesh_context* mesh_ctx = (struct nav_mesh_context*)malloc(sizeof(*mesh_ctx));
	memset(mesh_ctx,0,sizeof(*mesh_ctx));

	mesh_ctx->len = v_cnt;
	mesh_ctx->vertices = (struct vector3 *)malloc(sizeof(struct vector3) * mesh_ctx->len);
	memset(mesh_ctx->vertices,0,sizeof(struct vector3) * mesh_ctx->len);

	mesh_ctx->border_ctx.border_cap = 64;
	mesh_ctx->border_ctx.border_offset = 0;
	mesh_ctx->border_ctx.borders = (struct nav_border *)malloc(sizeof(struct nav_border) * mesh_ctx->border_ctx.border_cap);
	memset(mesh_ctx->border_ctx.borders,0,sizeof(struct nav_border) * mesh_ctx->border_ctx.border_cap);

	mesh_ctx->border_searcher = (struct nav_border_search_node**)malloc(sizeof(*mesh_ctx->border_searcher) * mesh_ctx->len);
	memset(mesh_ctx->border_searcher,0,sizeof(*mesh_ctx->border_searcher) * mesh_ctx->len);

	mesh_ctx->size = p_cnt;
	mesh_ctx->node = (struct nav_node *)malloc(sizeof(struct nav_node) * mesh_ctx->size);
	memset(mesh_ctx->node,0,sizeof(struct nav_node) * mesh_ctx->size);

	mesh_ctx->mask_ctx.size = 8;
	mesh_ctx->mask_ctx.mask = (int*)malloc(sizeof(int) * mesh_ctx->mask_ctx.size);
	for(int i = 0;i < mesh_ctx->mask_ctx.size;i++)
		set_mask(&mesh_ctx->mask_ctx,i,0);
	set_mask(&mesh_ctx->mask_ctx,0,1);

	mesh_ctx->lt.x = mesh_ctx->lt.y = mesh_ctx->lt.z = 0;
	mesh_ctx->br.x = mesh_ctx->br.y = mesh_ctx->br.z = 0;
	//¼ÓÔØ¶¥µã,ÕÒ³öµØÍ¼µÄ×óÉÏºÍÓÒÏÂµÄÁ½¸öÏîµã
	int i,j,k;
	for (i = 0;i < v_cnt;i++)
	{
		mesh_ctx->vertices[i].x = v[i][0];
		mesh_ctx->vertices[i].y = v[i][1];
		mesh_ctx->vertices[i].z = v[i][2];

		if (mesh_ctx->lt.x == 0)
			mesh_ctx->lt.x = mesh_ctx->vertices[i].x;
		else
		{
			if (mesh_ctx->vertices[i].x < mesh_ctx->lt.x)
				mesh_ctx->lt.x = mesh_ctx->vertices[i].x;
		}

		if (mesh_ctx->lt.z == 0)
			mesh_ctx->lt.z = mesh_ctx->vertices[i].z;
		else
		{
			if (mesh_ctx->vertices[i].z < mesh_ctx->lt.z)
				mesh_ctx->lt.z = mesh_ctx->vertices[i].z;
		}

		if (mesh_ctx->br.x == 0)
			mesh_ctx->br.x = mesh_ctx->vertices[i].x;
		else
		{
			if (mesh_ctx->vertices[i].x > mesh_ctx->br.x)
				mesh_ctx->br.x = mesh_ctx->vertices[i].x;
		}

		if (mesh_ctx->br.z == 0)
			mesh_ctx->br.z = mesh_ctx->vertices[i].z;
		else
		{
			if (mesh_ctx->vertices[i].z > mesh_ctx->br.z)
				mesh_ctx->br.z = mesh_ctx->vertices[i].z;
		}
	}

	mesh_ctx->width = mesh_ctx->br.x - mesh_ctx->lt.x;
	mesh_ctx->heigh = mesh_ctx->br.z - mesh_ctx->lt.z;

	//¼ÓÔØ¶à±ßÐÎË÷Òý
	for (i = 0;i < p_cnt;i++)
	{
		struct nav_node* node = &mesh_ctx->node[i];
		memset(node,0,sizeof(*node));
		node->id = i;

		node->size = p[i][0];

		node->border = (int*)malloc(node->size * sizeof(int));
		node->poly =(int*)malloc(node->size * sizeof(int));

		struct vector3 center;
		center.x = center.y = center.z = 0;

		node->link_border = -1;
		node->link_parent = NULL;

		for (j = 1;j <= node->size;j++)
		{
			node->poly[j-1] = p[i][j];
			center.x += mesh_ctx->vertices[node->poly[j-1]].x;
			center.y += mesh_ctx->vertices[node->poly[j-1]].y;
			center.z += mesh_ctx->vertices[node->poly[j-1]].z;
		}
		node->mask = p[i][node->size+1];
		node->center.x = center.x / node->size;
		node->center.y = center.y / node->size;
		node->center.z = center.z / node->size;

		//¶¥µãË³Ê±ÕëÅÅÐò
		vertex_sort(mesh_ctx,node);

		//Í¬Ê±Éú³ÉË³Ê±ÕëºÍÄæÊ±ÕëµÄ¶à±ßÐÎµÄ±ßborder,²¢¼ÇÂ¼±ßµÄÁ½±ß¶à±ßÐÎ
		for (k = 0; k < node->size;k++)
		{
			int k0 = k;
			int k1 = k + 1 >= node->size ? 0 : k + 1;
			
			int a = node->poly[k0];
			int b = node->poly[k1];

			struct nav_border* border = search_border(mesh_ctx, a, b);
			border_link_node(border,node->id);
			
			int border_id = border->id;
			node->border[k] = border_id;

			struct nav_border* border_opposite = search_border(mesh_ctx, b, a);
			border_link_node(border_opposite, node->id);
			border_opposite->opposite = border_id;
			
			border = get_border(mesh_ctx,border_id);
			border->opposite = border_opposite->id;
		}
	}

	release_border_searcher(mesh_ctx);

#ifdef USE_NAV_TILE
	create_tile(mesh_ctx);
#endif

	mesh_ctx->result.size = 8;
	mesh_ctx->result.offset = 0;
	mesh_ctx->result.wp = (struct vector3*)malloc(sizeof(struct vector3)*mesh_ctx->result.size);

	mesh_ctx->openlist = minheap_new(50 * 50, node_cmp);
	list_init(&mesh_ctx->closelist);
	list_init(&mesh_ctx->linked);

	return mesh_ctx;
}

void release_mesh(struct nav_mesh_context* ctx)
{
	free(ctx->vertices);
	free(ctx->border_ctx.borders);

	for(int i = 0;i < ctx->size;i++)
	{
		if (ctx->node[i].border != NULL)
			free(ctx->node[i].border);
		if (ctx->node[i].poly != NULL)
			free(ctx->node[i].poly);
	}
	free(ctx->node);
#ifdef USE_NAV_TILE
	release_tile(ctx);
#endif
	free(ctx->mask_ctx.mask);
	free(ctx->result.wp);
	minheap_delete(ctx->openlist);
	free(ctx);
}


//FIXME:共线问题
bool raycast(struct nav_mesh_context* ctx, struct vector3* pt0, struct vector3* pt1, struct vector3* result,search_dumper dumper, void* userdata)
{
	struct nav_node* curr_node = get_node_with_pos(ctx,pt0->x,pt0->y,pt0->z);

	struct vector3 vt10;
	vector3_sub(pt1,pt0,&vt10);

	while (curr_node)
	{
		if (inside_node(ctx, curr_node->id, pt1->x, pt1->y, pt1->z))
		{
			vector3_copy(result,pt1);
			return true;
		}

		bool crossed = false;
		for (int i = 0; i < curr_node->size; i++)
		{
			struct nav_border* border = get_border(ctx, curr_node->border[i]);

			struct vector3* pt3 = &ctx->vertices[border->a];
			struct vector3* pt4 = &ctx->vertices[border->b];

			struct vector3 vt30,vt40;
			vector3_sub(pt3,pt0,&vt30);
			vector3_sub(pt4,pt0,&vt40);

			double direct_a = cross(&vt30,&vt10);
			double direct_b = cross(&vt40,&vt10);

			if ((direct_a < 0 && direct_b > 0) || (direct_a == 0 && direct_b > 0) || (direct_a < 0 && direct_b == 0))
			{
				int next = -1;
				if (border->node[0] !=-1)
				{
					if (border->node[0] == curr_node->id)
						next = border->node[1];
					else
						next = border->node[0];
				}
				else
					assert(border->node[1] == curr_node->id);
				
				if (next == -1)
				{
					cross_point(pt3,pt4,pt1,pt0,result);
					return false;
				}
				else
				{
					struct nav_node* next_node = get_node(ctx,next);
					if (get_mask(ctx->mask_ctx, next_node->mask) == 0)
					{
						cross_point(pt3,pt4,pt1,pt0,result);
						return false;
					}
					if (dumper)
						dumper(userdata, next);

					crossed = true;
					curr_node = next_node;
					break;
				}
			}
		}
		if (!crossed)
			assert(0);
	}
	return false;
}


#define clear_node(n) do {n->link_parent = NULL; n->link_border = -1; n->F = n->G = n->H = 0; n->elt.index = 0; } while (0)


static inline void heap_clear(struct element* elt) 
{
	struct nav_node *node = cast_node(elt);
	clear_node(node);
}

#define reset(ctx) do \
{\
	struct nav_node * node = NULL; \
	while (node = (struct nav_node*)list_pop(&ctx->closelist)) \
	{\
		clear_node(node); \
	}\
	minheap_clear((ctx)->openlist, heap_clear); \
} while (0)



struct nav_node* next_border(struct nav_mesh_context* ctx,struct nav_node* node,struct vector3* wp,int *link_border)
{
	struct vector3 vt0,vt1;
	*link_border = node->link_border;
	while (*link_border != -1)
	{
		struct nav_border* border = get_border(ctx,*link_border);
		vector3_sub(&ctx->vertices[border->a],wp,&vt0);
		vector3_sub(&ctx->vertices[border->b],wp,&vt1);
		if ((vt0.x == 0 && vt0.z == 0) || (vt1.x == 0 && vt1.z == 0))
		{
			node = node->link_parent;
			*link_border = node->link_border;
		}
		else
			break;
	}
	if (*link_border != -1)
		return node;
	
	return NULL;
}

void path_init(struct nav_mesh_context* mesh_ctx)
{
	mesh_ctx->result.offset = 0;
}

void path_add(struct nav_mesh_context* mesh_ctx,struct vector3* wp)
{
	if (mesh_ctx->result.offset >= mesh_ctx->result.size)
	{
		int nsize = mesh_ctx->result.size * 2;
		struct vector3* owp = mesh_ctx->result.wp;
		mesh_ctx->result.wp = (struct vector3*)malloc(sizeof(struct vector3)*nsize);
		memcpy(mesh_ctx->result.wp,owp,mesh_ctx->result.size * sizeof(struct vector3));
		mesh_ctx->result.size = nsize;
		free(owp);
	}

	mesh_ctx->result.wp[mesh_ctx->result.offset].x = wp->x;
	mesh_ctx->result.wp[mesh_ctx->result.offset].z = wp->z;
	mesh_ctx->result.offset++;
}

void make_waypoint(struct nav_mesh_context* mesh_ctx,struct vector3* pt0,struct vector3* pt1,struct nav_node * node)
{
	path_add(mesh_ctx, pt1);

	struct vector3* pt_wp = pt1;

	int link_border = node->link_border;

	struct nav_border* border = get_border(mesh_ctx,link_border);

	struct vector3 pt_left,pt_right;
	vector3_copy(&pt_left,&mesh_ctx->vertices[border->a]);
	vector3_copy(&pt_right,&mesh_ctx->vertices[border->b]);

	struct vector3 vt_left,vt_right;
	vector3_sub(&pt_left,pt_wp,&vt_left);
	vector3_sub(&pt_right,pt_wp,&vt_right);

	struct nav_node* left_node = node->link_parent;
	struct nav_node* right_node = node->link_parent;

	struct nav_node* tmp = node->link_parent;
	while (tmp)
	{
		int link_border = tmp->link_border;
		if (link_border == -1)
		{
			struct vector3 tmp_target;
			tmp_target.x = pt0->x - pt_wp->x;
			tmp_target.z = pt0->z - pt_wp->z;

			double forward_a = cross(&vt_left,&tmp_target);
			double forward_b = cross(&vt_right,&tmp_target);

			if (forward_a < 0 && forward_b > 0)
			{
				path_add(mesh_ctx, pt0);
				break;
			}
			else
			{
				if (forward_a > 0 && forward_b > 0)
				{
					pt_wp->x = pt_left.x;
					pt_wp->z = pt_left.z;

					path_add(mesh_ctx, pt_wp);

					left_node = next_border(mesh_ctx,left_node,pt_wp,&link_border);
					if (left_node == NULL)
					{
						path_add(mesh_ctx, pt0);
						break;
					}
					
					border = get_border(mesh_ctx,link_border);
					pt_left.x = mesh_ctx->vertices[border->a].x;
					pt_left.z = mesh_ctx->vertices[border->a].z;

					pt_right.x = mesh_ctx->vertices[border->b].x;
					pt_right.z = mesh_ctx->vertices[border->b].z;

					vt_left.x = pt_left.x - pt_wp->x;
					vt_left.z = pt_left.z - pt_wp->z;

					vt_right.x = pt_right.x - pt_wp->x;
					vt_right.z = pt_right.z - pt_wp->z;

					tmp = left_node->link_parent;
					left_node = tmp;
					right_node = tmp;
					continue;
				}
				else if (forward_a < 0 && forward_b < 0)
				{
					pt_wp->x = pt_right.x;
					pt_wp->z = pt_right.z;

					path_add(mesh_ctx, pt_wp);

					right_node = next_border(mesh_ctx,right_node,pt_wp,&link_border);
					if (right_node == NULL)
					{
						path_add(mesh_ctx, pt0);
						break;
					}
					
					border = get_border(mesh_ctx,link_border);
					pt_left.x = mesh_ctx->vertices[border->a].x;
					pt_left.z = mesh_ctx->vertices[border->a].z;

					pt_right.x = mesh_ctx->vertices[border->b].x;
					pt_right.z = mesh_ctx->vertices[border->b].z;

					vt_left.x = pt_left.x - pt_wp->x;
					vt_left.z = pt_left.z - pt_wp->z;

					vt_right.x = pt_right.x - pt_wp->x;
					vt_right.z = pt_right.z - pt_wp->z;

					tmp = right_node->link_parent;
					left_node = tmp;
					right_node = tmp;
					continue;
				}
				break;
			}

		}

		border = get_border(mesh_ctx,link_border);

		struct vector3 tmp_pt_left,tmp_pt_right;
		vector3_copy(&tmp_pt_left,&mesh_ctx->vertices[border->a]);
		vector3_copy(&tmp_pt_right,&mesh_ctx->vertices[border->b]);

		struct vector3 tmp_vt_left,tmp_vt_right;
		vector3_sub(&tmp_pt_left,pt_wp,&tmp_vt_left);
		vector3_sub(&tmp_pt_right,pt_wp,&tmp_vt_right);

		double forward_left_a = cross(&vt_left,&tmp_vt_left);
		double forward_left_b = cross(&vt_right,&tmp_vt_left);
		double forward_right_a = cross(&vt_left,&tmp_vt_right);
		double forward_right_b = cross(&vt_right,&tmp_vt_right);

		if (forward_left_a < 0 && forward_left_b > 0)
		{
			left_node = tmp->link_parent;
			vector3_copy(&pt_left,&tmp_pt_left);
			vector3_sub(&pt_left,pt_wp,&vt_left);
		}

		if (forward_right_a < 0 && forward_right_b > 0)
		{
			right_node = tmp->link_parent;
			vector3_copy(&pt_right,&tmp_pt_right);
			vector3_sub(&pt_right,pt_wp,&vt_right);
		}

		if (forward_left_a > 0 && forward_left_b > 0 && forward_right_a > 0 && forward_right_b > 0)
		{
			vector3_copy(pt_wp,&pt_left);

			left_node = next_border(mesh_ctx,left_node,pt_wp,&link_border);
			if (left_node == NULL)
			{
				path_add(mesh_ctx, pt0);
				break;
			}
			
			border = get_border(mesh_ctx,link_border);
			vector3_copy(&pt_left,&mesh_ctx->vertices[border->a]);
			vector3_copy(&pt_right,&mesh_ctx->vertices[border->b]);

			vector3_sub(&mesh_ctx->vertices[border->a],pt_wp,&vt_left);
			vector3_sub(&mesh_ctx->vertices[border->b],pt_wp,&vt_right);

			path_add(mesh_ctx, pt_wp);

			tmp = left_node->link_parent;
			left_node = tmp;
			right_node = tmp;

			continue;
		}

		if (forward_left_a < 0 && forward_left_b < 0 && forward_right_a < 0 && forward_right_b < 0)
		{
			vector3_copy(pt_wp,&pt_right);

			right_node = next_border(mesh_ctx,right_node,pt_wp,&link_border);
			if (right_node == NULL)
			{
				path_add(mesh_ctx, pt0);
				break;
			}
			
			border = get_border(mesh_ctx,link_border);
			vector3_copy(&pt_left,&mesh_ctx->vertices[border->a]);
			vector3_copy(&pt_right,&mesh_ctx->vertices[border->b]);

			vector3_sub(&mesh_ctx->vertices[border->a],pt_wp,&vt_left);
			vector3_sub(&mesh_ctx->vertices[border->b],pt_wp,&vt_right);

			path_add(mesh_ctx, pt_wp);

			tmp = right_node->link_parent;
			left_node = tmp;
			right_node = tmp;
			continue;
		}

		tmp = tmp->link_parent;
	}
}

struct nav_path* astar_find(struct nav_mesh_context* mesh_ctx, struct vector3* pt_start, struct vector3* pt_over, search_dumper dumper, void* userdata)
{
	path_init(mesh_ctx);

	struct nav_node* node_start = get_node_with_pos(mesh_ctx, pt_start->x, pt_start->y, pt_start->z);
	struct nav_node* node_over = get_node_with_pos(mesh_ctx, pt_over->x, pt_over->y, pt_over->z);

	if (!node_start || !node_over)
		return NULL;

	if (node_start == node_over)
	{
		path_add(mesh_ctx, pt_start);
		path_add(mesh_ctx, pt_over);
		return &mesh_ctx->result;
	}

	vector3_copy(&node_start->pos, pt_start);
	
	minheap_push(mesh_ctx->openlist, &node_start->elt);

	struct nav_node* node_current = NULL;
	for (;;)
	{
		struct element* elt = minheap_pop(mesh_ctx->openlist);
		if (!elt)
		{
			reset(mesh_ctx);
			return NULL;
		}
		node_current = cast_node(elt);
		
		if (node_current == node_over)
		{
			make_waypoint(mesh_ctx, pt_start, pt_over, node_current);
			reset(mesh_ctx);
			clear_node(node_current);
			return &mesh_ctx->result;
		}

		list_push(&mesh_ctx->closelist, (struct list_node*)node_current);

		struct list* linked = get_link(mesh_ctx, node_current);
		if (linked)
		{
			struct nav_node* linked_node;
			while (linked_node = (struct nav_node*)list_pop(linked))
			{
				if (linked_node->elt.index)
				{
					double nG = node_current->G + G_COST(node_current, linked_node);
					if (nG < linked_node->G)
					{
						linked_node->G = nG;
						linked_node->F = linked_node->G + linked_node->H;
						linked_node->link_parent = node_current;
						linked_node->link_border = linked_node->reserve;
						minheap_change(mesh_ctx->openlist, &linked_node->elt);
					}
				}
				else
				{
					linked_node->G = node_current->G + G_COST(node_current, linked_node);
					linked_node->H = H_COST(linked_node,pt_over);
					linked_node->F = linked_node->G + linked_node->H;
					linked_node->link_parent = node_current;
					linked_node->link_border = linked_node->reserve;
					minheap_push(mesh_ctx->openlist, &linked_node->elt);
					if (dumper != NULL)
						dumper(userdata, linked_node->id);
				}
			}
		}
	}
}

struct vector3* around_movable(struct nav_mesh_context* ctx,double x,double z,double y, int range,search_dumper dumper,void* userdata)
{
#ifdef USE_NAV_TILE
	if (x < ctx->lt.x || x > ctx->br.x)
		return NULL;
	if (z < ctx->lt.z || z > ctx->br.z)
		return NULL;

	int x_index = x - ctx->lt.x;
	int z_index = z - ctx->lt.z;

	int r;
	for (r = 1; r <= range; ++r)
	{
		int x_min = x_index - r;
		int x_max = x_index + r;
		int z_min = z_index - r;
		int z_max = z_index + r;

		int x,z;

		int z_range[2] = { z_min, z_max };

		int j;
		for (j = 0; j < 2;j++)
		{
			z = z_range[j];
			if (z < 0 || z >= ctx->heigh)
				continue;
			
			int x_begin = x_min < 0 ? 0 : x_min;
			int x_end = x_max >= ctx->width ? ctx->width-1 : x_max;
			for (x = x_begin; x <= x_end; x++)
			{
				int index = x + z * ctx->width;
				struct nav_tile* tile = &ctx->tile[index];
				if (dumper)
					dumper(userdata, index);
				for (int i = 0; i < tile->offset; i++)
				{
					if (inside_node(ctx, tile->node[i], tile->center.x, tile->center.y, tile->center.z))
						return &tile->center;
				}
			}
		}

		int x_range[2] = { x_min, x_max };

		for (j = 0; j < 2;j++)
		{
			x = x_range[j];
			if (x < 0 || x >= ctx->width)
				continue;

			int z_begin = z_min + 1 < 0 ? 0 : z_min + 1;
			int z_end = z_max >= ctx->heigh ? ctx->heigh-1 : z_max;
			for (z = z_begin; z < z_end; z++)
			{
				int index = x + z * ctx->width;
				struct nav_tile* tile = &ctx->tile[index];
				if (dumper)
					dumper(userdata, index);
				for (int i = 0; i < tile->offset; i++)
				{
					if (inside_node(ctx, tile->node[i], tile->center.x, tile->center.y, tile->center.z))
						return &tile->center;
				}
			}
		}
	}
	return NULL;
#else
	return NULL;
#endif
}

bool point_movable(struct nav_mesh_context* ctx, double x, double z, double y)
{
	struct nav_node* node = get_node_with_pos(ctx, x, y, z);
	if (node)
		return true;
	return false;
}