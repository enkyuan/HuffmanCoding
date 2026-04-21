#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "huffman.h"

/* count the occurrences in a file */

long *countLetters(FILE *fp)
{
   long *asciiCount = (long *)malloc(sizeof(long)*ASCII_SIZE);
   if (asciiCount == NULL) {
      return NULL;
   }
   int ch;
   for (ch = 0; ch < ASCII_SIZE; ch++) {
      asciiCount[ch] = 0;
   }
   fseek(fp, 0, SEEK_SET);
   while ((ch = fgetc(fp)) != EOF) {
      asciiCount[ch] += 1;
   }
   return asciiCount;
}

static long creationCounter = 0;

static int compareLeafAscii(const TreeNode *a, const TreeNode *b)
{
   if (a->label < b->label) {
      return -1;
   }
   if (a->label > b->label) {
      return 1;
   }
   return 0;
}

static int compareInternalCreation(const TreeNode *a, const TreeNode *b)
{
   if (a->label < b->label) {
      return -1;
   }
   if (a->label > b->label) {
      return 1;
   }
   return 0;
}

static void fillCodes(const TreeNode *ptr, char *buffer, int depth, char **codes)
{
   if (ptr == NULL) {
      return;
   }
   if (isLeafNode(ptr)) {
      if (depth == 0) {
         buffer[0] = '0';
         buffer[1] = '\0';
      } else {
         buffer[depth] = '\0';
      }
      codes[ptr->label] = (char *)malloc((strlen(buffer) + 1) * sizeof(char));
      if (codes[ptr->label] != NULL) {
         strcpy(codes[ptr->label], buffer);
      }
      return;
   }
   buffer[depth] = '0';
   fillCodes(ptr->left, buffer, depth + 1, codes);
   buffer[depth] = '1';
   fillCodes(ptr->right, buffer, depth + 1, codes);
}

static void freeCodes(char **codes)
{
   int i;
   for (i = 0; i < ASCII_SIZE; i++) {
      free(codes[i]);
   }
}

typedef struct {
   unsigned char byte;
   int bitsFilled;
} BitWriter;

static void bitWriterInit(BitWriter *writer)
{
   writer->byte = 0;
   writer->bitsFilled = 0;
}

static void writeBit(FILE *fp, BitWriter *writer, int bit)
{
   writer->byte = (unsigned char)((writer->byte << 1) | (bit & 1));
   writer->bitsFilled += 1;
   if (writer->bitsFilled == 8) {
      fputc(writer->byte, fp);
      writer->byte = 0;
      writer->bitsFilled = 0;
   }
}

static void writeByte(FILE *fp, BitWriter *writer, unsigned char value)
{
   int i;
   for (i = 7; i >= 0; i--) {
      writeBit(fp, writer, (value >> i) & 1);
   }
}

static void writeHeaderTree(const TreeNode *ptr, FILE *fp, BitWriter *writer)
{
   if (ptr == NULL) {
      return;
   }
   if (isLeafNode(ptr)) {
      writeBit(fp, writer, 1);
      writeByte(fp, writer, (unsigned char)ptr->label);
      return;
   }
   writeBit(fp, writer, 0);
   writeHeaderTree(ptr->left, fp, writer);
   writeHeaderTree(ptr->right, fp, writer);
}

static void flushBitWriter(FILE *fp, BitWriter *writer)
{
   if (writer->bitsFilled > 0) {
      writer->byte = (unsigned char)(writer->byte << (8 - writer->bitsFilled));
      fputc(writer->byte, fp);
      writer->byte = 0;
      writer->bitsFilled = 0;
   }
}

static void sortCharsByCount(int *chars, int n, long *asciiCount)
{
   int i, j;
   for (i = 0; i < n - 1; i++) {
      for (j = i + 1; j < n; j++) {
         if (asciiCount[chars[j]] < asciiCount[chars[i]] ||
             (asciiCount[chars[j]] == asciiCount[chars[i]] && chars[j] < chars[i])) {
            int temp = chars[i];
            chars[i] = chars[j];
            chars[j] = temp;
         }
      }
   }
}

TreeNode *buildTreeNode(int label, TreeNode *left, TreeNode *right)
{
   TreeNode *node = (TreeNode *)malloc(sizeof(TreeNode));
   if (node == NULL) {
      return NULL;
   }
   node->label = label;
   node->left = left;
   node->right = right;
   if (left == NULL && right == NULL) {
      node->count = 0;
   } else {
      node->count = 0;
      if (left != NULL) {
         node->count += left->count;
      }
      if (right != NULL) {
         node->count += right->count;
      }
   }
   return node;
}

void freeHuffmanTree(TreeNode *ptr)
{
   if (ptr == NULL) {
      return;
   }
   freeHuffmanTree(ptr->left);
   freeHuffmanTree(ptr->right);
   free(ptr);
}

void huffmanPrint(const TreeNode *ptr, FILE *fp)
{
   char *codes[ASCII_SIZE];
   char buffer[ASCII_SIZE + 1];
   int i;
   for (i = 0; i < ASCII_SIZE; i++) {
      codes[i] = NULL;
   }
   fillCodes(ptr, buffer, 0, codes);
   for (i = 0; i < ASCII_SIZE; i++) {
      if (codes[i] != NULL) {
         fprintf(fp, "%c:%s\n", i, codes[i]);
      }
   }
   freeCodes(codes);
}

int isLeafNode(const TreeNode *node)
{
   if (node == NULL) {
      return 0;
   }
   return node->left == NULL && node->right == NULL;
}

long treeNodeCount(TreeNode *node)
{
   if (node == NULL) {
      return 0;
   }
   return node->count;
}

int treeNodeCompare(TreeNode *tp1, TreeNode *tp2)
{
   if (tp1 == NULL && tp2 == NULL) {
      return 0;
   }
   if (tp1 == NULL) {
      return -1;
   }
   if (tp2 == NULL) {
      return 1;
   }
   if (tp1->count < tp2->count) {
      return -1;
   }
   if (tp1->count > tp2->count) {
      return 1;
   }

   if (isLeafNode(tp1) && !isLeafNode(tp2)) {
      return -1;
   }
   if (!isLeafNode(tp1) && isLeafNode(tp2)) {
      return 1;
   }
   if (isLeafNode(tp1) && isLeafNode(tp2)) {
      return compareLeafAscii(tp1, tp2);
   }
   return compareInternalCreation(tp1, tp2);
}

ListNode *addListNode(ListNode **list, TreeNode *new_object,
                  int (*cmpFunction)(TreeNode *, TreeNode *))
{
   ListNode *newNode;
   ListNode *curr;
   if (list == NULL || new_object == NULL || cmpFunction == NULL) {
      return NULL;
   }

   newNode = (ListNode *)malloc(sizeof(ListNode));
   if (newNode == NULL) {
      return NULL;
   }
   newNode->ptr = new_object;
   newNode->next = NULL;

   if (*list == NULL || cmpFunction(new_object, (*list)->ptr) < 0) {
      newNode->next = *list;
      *list = newNode;
      return newNode;
   }

   curr = *list;
   while (curr->next != NULL && cmpFunction(new_object, curr->next->ptr) >= 0) {
      curr = curr->next;
   }
   newNode->next = curr->next;
   curr->next = newNode;
   return newNode;
}

ListNode *removeListNode(ListNode **list)
{
   ListNode *removed;
   if (list == NULL || *list == NULL) {
      return NULL;
   }
   removed = *list;
   *list = (*list)->next;
   removed->next = NULL;
   return removed;
}

void freeList(ListNode *list)
{
   ListNode *next;
   while (list != NULL) {
      next = list->next;
      free(list);
      list = next;
   }
}

void printList(const ListNode *list, FILE *fp)
{
   while (list != NULL) {
      fprintf(fp, "%ld ", list->ptr->count);
      list = list->next;
   }
   fprintf(fp, "\n");
}

TreeNode *buildHuffmanTree(ListNode *list)
{
   while (list != NULL && list->next != NULL) {
      ListNode *first = removeListNode(&list);
      ListNode *second = removeListNode(&list);
      TreeNode *combined = buildTreeNode(-(int)(++creationCounter), first->ptr, second->ptr);
      if (combined == NULL) {
         free(first);
         free(second);
         freeList(list);
         return NULL;
      }
      free(first);
      free(second);
      addListNode(&list, combined, treeNodeCompare);
   }

   if (list == NULL) {
      return NULL;
   }

   TreeNode *root = list->ptr;
   free(list);
   return root;
}

static int writeSortedFile(const char *filename, long *asciiCount)
{
   FILE *fp = fopen(filename, "w");
   int chars[ASCII_SIZE];
   int n = 0;
   int i;

   if (fp == NULL) {
      return 0;
   }

   for (i = 0; i < ASCII_SIZE; i++) {
      if (asciiCount[i] > 0) {
         chars[n++] = i;
      }
   }

   sortCharsByCount(chars, n, asciiCount);

   for (i = 0; i < n; i++) {
      fprintf(fp, "%c:%ld\n", chars[i], asciiCount[chars[i]]);
   }

   fclose(fp);
   return 1;
}

static int writeHuffmanFile(const char *filename, const TreeNode *root)
{
   FILE *fp = fopen(filename, "w");
   if (fp == NULL) {
      return 0;
   }
   huffmanPrint(root, fp);
   fclose(fp);
   return 1;
}

static int writeHeaderFile(const char *filename, const TreeNode *root)
{
   FILE *fp = fopen(filename, "wb");
   BitWriter writer;
   if (fp == NULL) {
      return 0;
   }

   bitWriterInit(&writer);
   writeHeaderTree(root, fp, &writer);
   writeBit(fp, &writer, 0);
   flushBitWriter(fp, &writer);

   fclose(fp);
   return 1;
}

// You main function takes exactly four inputs
// argv[1]: input file name - for example, testcases/gophers
// argv[2]: output file 1 name - to store the sorted characters, for example, gophers_sorted
// argv[3]: output file 2 name - to store the huffman code of each characters, for example, gophers_huffman
// argv[4]: output file 3 name - to store the header information, for example, gophers_header
int main(int argc, char **argv)
{
   if (argc != 5) {
    printf("Not enough arguments");
    return EXIT_FAILURE;
   }
   FILE * inFile = fopen(argv[1], "r");
   if (inFile == NULL) {
      fprintf(stderr, "can't open the input file.  Quit.\n");
      return EXIT_FAILURE;
   }

   /* read and count the occurrences of characters */
   long *asciiCount = countLetters(inFile);
   fclose(inFile);

   if (asciiCount == NULL) {
      fprintf(stderr, "cannot allocate memory to count the characters in input file.  Quit.\n");
      return EXIT_FAILURE;
   }

   // Your code should go here

   ListNode *list = NULL;
   TreeNode *root = NULL;
   int i;

   for (i = 0; i < ASCII_SIZE; i++) {
      if (asciiCount[i] > 0) {
         TreeNode *leaf = buildTreeNode(i, NULL, NULL);
         if (leaf == NULL) {
            fprintf(stderr, "cannot allocate memory for huffman tree.  Quit.\n");
            freeList(list);
            free(asciiCount);
            return EXIT_FAILURE;
         }
         leaf->count = asciiCount[i];
         if (addListNode(&list, leaf, treeNodeCompare) == NULL) {
            fprintf(stderr, "cannot allocate memory for linked list.  Quit.\n");
            free(leaf);
            while (list != NULL) {
               ListNode *node = removeListNode(&list);
               freeHuffmanTree(node->ptr);
               free(node);
            }
            free(asciiCount);
            return EXIT_FAILURE;
         }
      }
   }

   if (!writeSortedFile(argv[2], asciiCount)) {
      fprintf(stderr, "cannot open output file.  Quit.\n");
      while (list != NULL) {
         ListNode *node = removeListNode(&list);
         freeHuffmanTree(node->ptr);
         free(node);
      }
      free(asciiCount);
      return EXIT_FAILURE;
   }

   root = buildHuffmanTree(list);
   if (root == NULL) {
      fprintf(stderr, "cannot build huffman tree.  Quit.\n");
      free(asciiCount);
      return EXIT_FAILURE;
   }

   if (!writeHuffmanFile(argv[3], root)) {
      fprintf(stderr, "cannot open output file.  Quit.\n");
      freeHuffmanTree(root);
      free(asciiCount);
      return EXIT_FAILURE;
   }

   if (!writeHeaderFile(argv[4], root)) {
      fprintf(stderr, "cannot open output file.  Quit.\n");
      freeHuffmanTree(root);
      free(asciiCount);
      return EXIT_FAILURE;
   }

   freeHuffmanTree(root);
   free(asciiCount);

   return EXIT_SUCCESS;
}
