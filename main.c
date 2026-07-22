#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "b_plus_tree.h"
#include "disk_operations.h"
//#include "cache.h"

#define GREEN  "\033[1;32m"
#define RED    "\033[1;31m"
#define YELLOW "\033[1;33m"
#define RESET  "\033[0m"

#define MAX_KEY 1000
#define OPS 50000

static int tests_run = 0;
static int tests_failed = 0;

#define ASSERT(expr)                                              \
do {                                                              \
    tests_run++;                                                  \
    if (!(expr)) {                                                \
        printf(RED "\nASSERT FAILED\n" RESET);                    \
        printf("File : %s\n", __FILE__);                          \
        printf("Line : %d\n", __LINE__);                          \
        printf("Expr : %s\n", #expr);                             \
        tests_failed++;                                           \
        exit(EXIT_FAILURE);                                       \
    }                                                             \
} while (0)

#define TEST(name)                                                \
printf(YELLOW "\nRunning %-45s" RESET, name);

#define PASS()                                                    \
printf(GREEN "PASS\n" RESET);

static void summary(void)
{
    printf("\n");
    printf("----------------------------------------\n");

    if(tests_failed==0)
        printf(GREEN "ALL %d TESTS PASSED\n" RESET,tests_run);
    else
        printf(RED "%d TESTS FAILED\n" RESET,tests_failed);

    printf("----------------------------------------\n");
}

static void delete_database(void)
{
	 save_database();
    remove("mydatabase.db");
}

static Location make_location(int key)
{
    Location l;
	char* name = "name";
	char* email = "email_@test.com";

    l = allocate_location(key, name, email);

    return l;
}

static void verify_slot(Slot* s,int key)
{
    ASSERT(s!=NULL);

    ASSERT(s->id==key);

    char expected[128];

    sprintf(expected,"name");
    ASSERT(strcmp(expected,s->name)==0);

    sprintf(expected,"email_@test.com");
    ASSERT(strcmp(expected,s->email)==0);
}

static void test_empty_tree(void)
{
    TEST("Create Empty Tree");

    delete_database();

    start_database_page();
    B_Tree* tree = btree_create_tree();

    ASSERT(tree!=NULL);

    Header* h = get_page(tree->root_page,READ);

    ASSERT(h!=NULL);

    ASSERT(h->type==LEAF_PAGE);

    LeafNode* leaf = get_page(tree->root_page,READ);

    ASSERT(leaf->count==0);
    ASSERT(leaf->parent==INVALID_PAGE);
    ASSERT(leaf->next_leaf==INVALID_PAGE);

    PASS();

	btree_delete_tree(tree);
}

static void test_single_insert(void)
{
    TEST("Single Insert");

	 printf("deleting\n");
    delete_database();
	 printf("deleted\n");

    start_database_page();
    B_Tree* tree = btree_create_tree();

    Location l = make_location(42);

    btree_insert_key(tree, 42, l);

    Slot* s = btree_search_entry(tree,42);

    verify_slot(s,42);

    PASS();

	btree_delete_tree(tree);
}

static void test_search_missing(void)
{
    TEST("Search Missing");

    delete_database();

    start_database_page();
    B_Tree* tree = btree_create_tree();

    Location l = make_location(10);

    btree_insert_key(tree,10,l);

    ASSERT(btree_search_entry(tree,999)==NULL);

    PASS();

	btree_delete_tree(tree);
}

static void test_duplicate_insert(void)
{
    TEST("Duplicate Insert");

    delete_database();

    start_database_page();
    B_Tree* tree = btree_create_tree();

    Location l=make_location(7);

    btree_insert_key(tree,7,l);
    btree_insert_key(tree,7,l);

    LeafNode* leaf=get_page(tree->root_page,READ);

    ASSERT(leaf->count==1);

    PASS();

	btree_delete_tree(tree);
}

/*----------------------------------------------------------
    Helper verification routines
----------------------------------------------------------*/

static void verify_leaf_sorted(LeafNode *leaf)
{
    ASSERT(leaf != NULL);

    for (uint32_t i = 1; i < leaf->count; i++)
    {
        ASSERT(leaf->entries[i-1].key < leaf->entries[i].key);
    }
}

static void verify_search_range(B_Tree *tree, int first, int last)
{
    for (int i = first; i <= last; i++)
    {
        Slot *s = btree_search_entry(tree, i);
        verify_slot(s, i);
    }
}

static void insert_range(B_Tree *tree, int first, int last)
{
    for (int i = first; i <= last; i++)
    {
        Location l = make_location(i);
        btree_insert_key(tree, i, l);
    }
}

static void test_fill_single_leaf(void)
{
    TEST("Fill Single Leaf");

    delete_database();

    start_database_page();
    B_Tree *tree = btree_create_tree();

    insert_range(tree,0, LEAF_MAX - 1);

    Header *h = get_page(tree->root_page,READ);

    ASSERT(h->type == LEAF_PAGE);

    LeafNode *leaf = get_page(tree->root_page,READ);

    ASSERT(leaf->count == LEAF_MAX);

    verify_leaf_sorted(leaf);

    verify_search_range(tree,0,LEAF_MAX-1);

    PASS();

	btree_delete_tree(tree);
}

static void test_first_leaf_split(void)
{
    TEST("First Leaf Split");

    delete_database();

    start_database_page();
    B_Tree *tree = btree_create_tree();

    insert_range(tree,0,LEAF_MAX);

    Header *h = get_page(tree->root_page,READ);

    ASSERT(h->type == INTERNAL_PAGE);

    InternalNode *root = get_page(tree->root_page,READ);

    ASSERT(root->count == 1);

    ASSERT(root->pages[0] != INVALID_PAGE);
    ASSERT(root->pages[1] != INVALID_PAGE);

    LeafNode *left =
        get_page(root->pages[0],READ);

    LeafNode *right =
        get_page(root->pages[1],READ);

    ASSERT(left != NULL);
    ASSERT(right != NULL);

    ASSERT(left->parent == tree->root_page);
    ASSERT(right->parent == tree->root_page);

    ASSERT(left->next_leaf == root->pages[1]);

    verify_leaf_sorted(left);
    verify_leaf_sorted(right);

    verify_search_range(tree,0,LEAF_MAX);

    PASS();

	 btree_delete_tree(tree);
}

static void test_sequential_insert_5000(void)
{
    TEST("Sequential Insert (5000)");

    delete_database();

    start_database_page();
    B_Tree *tree = btree_create_tree();

    insert_range(tree,0,4999);

    verify_search_range(tree,0,4999);

    PASS();

	btree_delete_tree(tree);
}

static void test_reverse_insert(void)
{
    TEST("Reverse Insert");

    delete_database();

    start_database_page();
    B_Tree *tree = btree_create_tree();

    for(int i=5000;i>=0;i--)
    {
        Location l = make_location(i);
        btree_insert_key(tree,i,l);
    }

    verify_search_range(tree,0,5000);

    PASS();

	btree_delete_tree(tree);
}

static void shuffle(int *array,int n)
{
    for(int i=n-1;i>0;i--)
    {
        int j = rand()%(i+1);

        int t=array[i];
        array[i]=array[j];
        array[j]=t;
    }
}

static void test_random_insert(void)
{
    TEST("Random Insert");

    delete_database();

    start_database_page();
    B_Tree *tree = btree_create_tree();

    int n = 5000;

    int *keys = malloc(sizeof(int)*n);

    ASSERT(keys != NULL);

    for(int i=0;i<n;i++)
        keys[i]=i;

    shuffle(keys,n);

    for(int i=0;i<n;i++)
    {
        Location l = make_location(keys[i]);
        btree_insert_key(tree,keys[i],l);
    }

    verify_search_range(tree,0,n-1);

    free(keys);

    PASS();

	btree_delete_tree(tree);
}

static void test_single_delete(void)
{
    TEST("Single Delete");

    delete_database();

    start_database_page();
    B_Tree *tree = btree_create_tree();

    Location l = make_location(10);

    ASSERT(btree_insert_key(tree, 10, l));
    ASSERT(btree_search_entry(tree, 10) != NULL);

    ASSERT(btree_delete_key(tree, 10));
    ASSERT(btree_search_entry(tree, 10) == NULL);

    PASS();

    btree_delete_tree(tree);
}

static void test_delete_missing(void)
{
    TEST("Delete Missing");

    delete_database();

    start_database_page();
    B_Tree *tree = btree_create_tree();

    Location l = make_location(10);

    ASSERT(btree_insert_key(tree, 10, l));

    ASSERT(!btree_delete_key(tree, 20));

    ASSERT(btree_search_entry(tree, 10) != NULL);

    PASS();

    btree_delete_tree(tree);
}

static void test_sequential_delete(void)
{
    TEST("Sequential Delete");

    delete_database();

    start_database_page();
    B_Tree *tree = btree_create_tree();

    int n = 5000;

    for(int i = 0; i < n; i++)
    {
        Location l = make_location(i);
        ASSERT(btree_insert_key(tree, i, l));
    }

    for(int i = 0; i < n; i++) {
        ASSERT(btree_delete_key(tree, i));
	}

    for(int i = 0; i < n; i++)
        ASSERT(btree_search_entry(tree, i) == NULL);

    PASS();

    btree_delete_tree(tree);
}

static void test_reverse_delete(void)
{
    TEST("Reverse Delete");

    delete_database();

    start_database_page();
    B_Tree *tree = btree_create_tree();

    int n = 5000;

    for(int i = 0; i < n; i++)
    {
        Location l = make_location(i);
        ASSERT(btree_insert_key(tree, i, l));
    }

    for(int i = n - 1; i >= 0; i--) {
        ASSERT(btree_delete_key(tree, i));
    }

    for(int i = 0; i < n; i++)
        ASSERT(btree_search_entry(tree, i) == NULL);

    PASS();

    btree_delete_tree(tree);
}

static void test_random_delete(void)
{
    TEST("Random Delete");

    delete_database();

    start_database_page();
    B_Tree *tree = btree_create_tree();

    int n = 5000;

    int *keys = malloc(sizeof(int) * n);

    ASSERT(keys != NULL);

    for(int i = 0; i < n; i++)
        keys[i] = i;

    shuffle(keys, n);

    for(int i = 0; i < n; i++)
    {
        Location l = make_location(keys[i]);
        ASSERT(btree_insert_key(tree, keys[i], l));
    }

    shuffle(keys, n);

    for(int i = 0; i < n; i++)
        ASSERT(btree_delete_key(tree, keys[i]));

    for(int i = 0; i < n; i++)
        ASSERT(btree_search_entry(tree, i) == NULL);

    free(keys);

    PASS();

    btree_delete_tree(tree);
}

static void test_delete_every_other(void)
{
    TEST("Delete Every Other");

    delete_database();

    start_database_page();
    B_Tree *tree = btree_create_tree();

    int n = 5000;

    for(int i = 0; i < n; i++)
    {
        Location l = make_location(i);
        ASSERT(btree_insert_key(tree, i, l));
    }

    for(int i = 0; i < n; i += 2)
        ASSERT(btree_delete_key(tree, i));

    for(int i = 0; i < n; i++)
    {
        if(i % 2 == 0)
            ASSERT(btree_search_entry(tree, i) == NULL);
        else
            ASSERT(btree_search_entry(tree, i) != NULL);
    }

    PASS();

    btree_delete_tree(tree);
}

static void test_reinsert_after_delete(void)
{
    TEST("Reinsert After Delete");

    delete_database();

    start_database_page();
    B_Tree *tree = btree_create_tree();

    int n = 5000;

    for(int i = 0; i < n; i++)
    {
        Location l = make_location(i);
        ASSERT(btree_insert_key(tree, i, l));
    }

    for(int i = 0; i < n; i++)
        ASSERT(btree_delete_key(tree, i));

    for(int i = 0; i < n; i++)
    {
        Location l = make_location(i);
        ASSERT(btree_insert_key(tree, i, l));
    }

    verify_search_range(tree, 0, n - 1);

    PASS();

    btree_delete_tree(tree);
}

static void test_random_fuzz(void)
{
    TEST("Random Fuzz");

    delete_database();

    start_database_page();

    B_Tree *tree = btree_create_tree();

    bool present[MAX_KEY];
    memset(present, 0, sizeof(present));

    srand(12345);

    for(int i = 0; i < OPS; i++)
    {
        int key = rand() % MAX_KEY;

        if(rand() % 2)
        {
            Location l = make_location(key);

            bool expected = !present[key];
            ASSERT(btree_insert_key(tree, key, l) == expected);

            if(expected)
                present[key] = true;
        }
        else
        {
            bool expected = present[key];
            ASSERT(btree_delete_key(tree, key) == expected);

            if(expected)
                present[key] = false;
        }

        for(int k = 0; k < MAX_KEY; k++)
        {
            Slot *s = btree_search_entry(tree, k);

            if(present[k])
                ASSERT(s != NULL);
            else
                ASSERT(s == NULL);
        }
    }

    PASS();

    btree_delete_tree(tree);
}

static void test_random_cycles(void)
{
    TEST("Random Cycles");

    delete_database();

    start_database_page();

    B_Tree *tree = btree_create_tree();

    srand(42);

    for(int cycle = 0; cycle < 25; cycle++)
    {
        int keys[5000];

        for(int i = 0; i < 5000; i++)
            keys[i] = i;

        shuffle(keys, 5000);

        for(int i = 0; i < 5000; i++)
        {
            Location l = make_location(keys[i]);
            ASSERT(btree_insert_key(tree, keys[i], l));
        }

        verify_search_range(tree,0,4999);

        shuffle(keys,5000);

        for(int i = 0; i < 5000; i++)
            ASSERT(btree_delete_key(tree, keys[i]));

        ASSERT(tree->root_page != INVALID_PAGE);
    }

    PASS();

    btree_delete_tree(tree);
}

static void verify_node(Page page,
                        int32_t min,
                        int32_t max)
{
    void *p = get_page(page, READ);
    Header *h = (Header *)p;

    switch(h->type)
    {
        case LEAF_PAGE:
        {
            LeafNode *n = p;

            ASSERT(n->count <= LEAF_MAX);

            for(int i=1;i<n->count;i++)
                ASSERT(n->entries[i-1].key < n->entries[i].key);

            if(min != INT32_MIN)
                ASSERT(n->entries[0].key >= min);

            if(max != INT32_MAX)
                ASSERT(n->entries[n->count-1].key < max);

            break;
        }

        case INTERNAL_PAGE:
        {
            InternalNode *n = p;

            ASSERT(n->count <= INTERNAL_MAX);

            for(int i=1;i<n->count;i++)
                ASSERT(n->keys[i-1] < n->keys[i]);

            verify_node(n->pages[0],min,n->keys[0]);

            for(int i=1;i<n->count;i++)
                verify_node(n->pages[i],n->keys[i-1],n->keys[i]);

            verify_node(n->pages[n->count],
                        n->keys[n->count-1],
                        max);

            break;
        }
    }
}

static void test_tree_verifier(void)
{
    TEST("Tree Verifier");

    delete_database();

    start_database_page();

    B_Tree *tree = btree_create_tree();

    for(int i=0;i<10000;i++)
    {
        Location l = make_location(i);
        ASSERT(btree_insert_key(tree,i,l));
    }

    verify_node(tree->root_page,
                INT32_MIN,
                INT32_MAX);

    PASS();

    btree_delete_tree(tree);
}

static void test_leaf_chain(void)
{
    TEST("Leaf Chain");

    delete_database();

    start_database_page();

    B_Tree *tree = btree_create_tree();

    for(int i=0;i<10000;i++)
    {
        Location l = make_location(i);
        ASSERT(btree_insert_key(tree,i,l));
    }

    Page p = tree->root_page;

    while(1)
    {
        void *page = get_page(p,READ);
        Header *h = page;

        if(h->type == LEAF_PAGE)
            break;

        InternalNode *n = page;
        p = n->pages[0];
    }

    int expected = 0;

    while(p != INVALID_PAGE)
    {
        LeafNode *leaf = get_page(p,READ);

        for(int i=0;i<leaf->count;i++)
            ASSERT(leaf->entries[i].key == expected++);

        p = leaf->next_leaf;
    }

    ASSERT(expected == 10000);

    PASS();

    btree_delete_tree(tree);
}

static void test_random_stress(void)
{
    TEST("Random Stress");

    delete_database();

    start_database_page();

    B_Tree *tree = btree_create_tree();

    srand(1234);

    for(int cycle=0; cycle<100; cycle++)
    {
        int n = 1000;

        int keys[1000];

        for(int i=0;i<n;i++)
            keys[i]=i;

        shuffle(keys,n);

        for(int i=0;i<n;i++)
        {
            Location l = make_location(keys[i]);
            ASSERT(btree_insert_key(tree,keys[i],l));
        }

        shuffle(keys,n);

        for(int i=0;i<n;i++)
            ASSERT(btree_delete_key(tree,keys[i]));

        verify_node(tree->root_page,
                    INT32_MIN,
                    INT32_MAX);
    }

    PASS();

    btree_delete_tree(tree);
}

int main(void)
{
    srand((unsigned)time(NULL));

    /*test_empty_tree();
    test_single_insert();
    test_search_missing();
    test_duplicate_insert();

    test_fill_single_leaf();
    test_first_leaf_split();*/

    //test_sequential_insert_5000();

    /*test_reverse_insert();

    test_random_insert();

    test_single_delete();
    test_delete_missing();
    test_sequential_delete();
    test_reverse_delete();
    test_random_delete();
    test_delete_every_other();
    test_reinsert_after_delete();

    test_random_fuzz();
    test_random_cycles();
    test_leaf_chain();
    test_random_stress();*/

	 /*test_first_leaf_split();

    summary();

	 printf("next = %u\n", db_information->next);
	 printf("root = %u\n", db_information->root_page);*/

	 //save_database();
	 start_database_page();
	 
	 B_Tree* b = start_btree(db_information->root_page);

	 Slot* s = btree_search_entry(b, 3289);
	 if (s != NULL) {
		printf("Name: %s\n", s->name);
		printf("Email: %s\n", s->email);
	 }

    return 0;
}
