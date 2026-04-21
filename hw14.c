#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "huffman.h"

/* count the occurrences in a file */

long *countLetters(FILE *fp)
{
   long *asciiCount = (long *)malloc(sizeof(long) * ASCII_SIZE);
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

static long internalCreationOrder = 0;

static void sortCharsByCount(int *chars, int n, long *asciiCount)
{
   int i;
   int j;

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

static void freeListAndTrees(ListNode *list)
{
   while (list != NULL) {
      ListNode *next = list->next;
      freeHuffmanTree(list->ptr);
      free(list);
      list = next;
   }
}

static void buildCodes(const TreeNode *ptr, char *path, int depth, char **codes)
{
   if (ptr == NULL) {
      return;
   }

   if (isLeafNode(ptr)) {
      if (depth == 0) {
         path[0] = '0';
         path[1] = '\0';
      } else {
         path[depth] = '\0';
      }

      codes[ptr->label] = (char *)malloc(strlen(path) + 1);
      if (codes[ptr->label] != NULL) {
         strcpy(codes[ptr->label], path);
      }
      return;
   }

   path[depth] = '0';
   buildCodes(ptr->left, path, depth + 1, codes);

   path[depth] = '1';
   buildCodes(ptr->right, path, depth + 1, codes);
}

typedef struct {
   unsigned char currentByte;
   int bitsUsed;
} BitWriter;

static void initBitWriter(BitWriter *writer)
{
   writer->currentByte = 0;
   writer->bitsUsed = 0;
}

static void writeOneBit(FILE *fp, BitWriter *writer, int bit)
{
   writer->currentByte = (unsigned char)((writer->currentByte << 1) | (bit & 1));
   writer->bitsUsed += 1;

   if (writer->bitsUsed == 8) {
      fputc(writer->currentByte, fp);
      writer->currentByte = 0;
      writer->bitsUsed = 0;
   }
}

static void writeEightBits(FILE *fp, BitWriter *writer, unsigned char value)
{
   int shift;
   for (shift = 7; shift >= 0; shift--) {
      writeOneBit(fp, writer, (value >> shift) & 1);
   }
}

static void writeHeaderPreorder(const TreeNode *ptr, FILE *fp, BitWriter *writer)
{
   if (ptr == NULL) {
      return;
   }

   if (isLeafNode(ptr)) {
      writeOneBit(fp, writer, 1);
      writeEightBits(fp, writer, (unsigned char)ptr->label);
      return;
   }

   writeOneBit(fp, writer, 0);
   writeHeaderPreorder(ptr->left, fp, writer);
   writeHeaderPreorder(ptr->right, fp, writer);
}

static void flushBitWriter(FILE *fp, BitWriter *writer)
{
   if (writer->bitsUsed > 0) {
      writer->currentByte = (unsigned char)(writer->currentByte << (8 - writer->bitsUsed));
      fputc(writer->currentByte, fp);
      writer->currentByte = 0;
      writer->bitsUsed = 0;
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
   node->count = 0;

   if (left != NULL) {
      node->count += left->count;
   }
   if (right != NULL) {
      node->count += right->count;
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

int isLeafNode(const TreeNode *node)
{
   if (node == NULL) {
      return 0;
   }
   return (node->left == NULL && node->right == NULL);
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
      if (tp1->label < tp2->label) {
         return -1;
      }
      if (tp1->label > tp2->label) {
         return 1;
      }
      return 0;
   }

   if (tp1->label > tp2->label) {
      return -1;
   }
   if (tp1->label < tp2->label) {
      return 1;
   }
   return 0;
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
   while (list != NULL) {
      ListNode *next = list->next;
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
      ListNode *firstNode = removeListNode(&list);
      ListNode *secondNode = removeListNode(&list);
      TreeNode *combined;

      if (firstNode == NULL || secondNode == NULL) {
         free(firstNode);
         free(secondNode);
         return NULL;
      }

      combined = buildTreeNode(-(int)(++internalCreationOrder), firstNode->ptr, secondNode->ptr);
      free(firstNode);
      free(secondNode);

      if (combined == NULL) {
         freeListAndTrees(list);
         return NULL;
      }

      if (addListNode(&list, combined, treeNodeCompare) == NULL) {
         freeHuffmanTree(combined);
         freeListAndTrees(list);
         return NULL;
      }
   }

   if (list == NULL) {
      return NULL;
   }

   TreeNode *root = list->ptr;
   free(list);
   return root;
}

void huffmanPrint(const TreeNode *ptr, FILE *fp)
{
   char *codes[ASCII_SIZE];
   char buffer[ASCII_SIZE + 1];
   int chars[ASCII_SIZE];
   int count = 0;
   int i;

   for (i = 0; i < ASCII_SIZE; i++) {
      codes[i] = NULL;
   }

   buildCodes(ptr, buffer, 0, codes);

   for (i = 0; i < ASCII_SIZE; i++) {
      if (codes[i] != NULL) {
         chars[count++] = i;
      }
   }

   sortCharsByCount(chars, count, (long [ASCII_SIZE]){0});

   for (i = 0; i < count; i++) {
      fprintf(fp, "%c:%s\n", chars[i], codes[chars[i]]);
   }

   for (i = 0; i < ASCII_SIZE; i++) {
      free(codes[i]);
   }
}

static int writeSortedOutput(const char *filename, long *asciiCount)
{
   FILE *fp;
   int chars[ASCII_SIZE];
   int count = 0;
   int i;

   fp = fopen(filename, "w");
   if (fp == NULL) {
      return 0;
   }

   for (i = 0; i < ASCII_SIZE; i++) {
      if (asciiCount[i] > 0) {
         chars[count++] = i;
      }
   }

   sortCharsByCount(chars, count, asciiCount);

   for (i = 0; i < count; i++) {
      fprintf(fp, "%c:%ld\n", chars[i], asciiCount[chars[i]]);
   }

   fclose(fp);
   return 1;
}

static int writeHuffmanOutput(const char *filename, const TreeNode *root, long *asciiCount)
{
   FILE *fp;
   char *codes[ASCII_SIZE];
   char buffer[ASCII_SIZE + 1];
   int chars[ASCII_SIZE];
   int count = 0;
   int i;

   fp = fopen(filename, "w");
   if (fp == NULL) {
      return 0;
   }

   for (i = 0; i < ASCII_SIZE; i++) {
      codes[i] = NULL;
   }

   buildCodes(root, buffer, 0, codes);

   for (i = 0; i < ASCII_SIZE; i++) {
      if (codes[i] != NULL) {
         chars[count++] = i;
      }
   }

   sortCharsByCount(chars, count, asciiCount);

   for (i = 0; i < count; i++) {
      fprintf(fp, "%c:%s\n", chars[i], codes[chars[i]]);
   }

   for (i = 0; i < ASCII_SIZE; i++) {
      free(codes[i]);
   }

   fclose(fp);
   return 1;
}

static int writeHeaderOutput(const char *filename, const TreeNode *root)
{
   FILE *fp;
   BitWriter writer;

   fp = fopen(filename, "wb");
   if (fp == NULL) {
      return 0;
   }

   initBitWriter(&writer);
   writeHeaderPreorder(root, fp, &writer);
   writeOneBit(fp, &writer, 0);
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
            freeListAndTrees(list);
            free(asciiCount);
            return EXIT_FAILURE;
         }

         leaf->count = asciiCount[i];

         if (addListNode(&list, leaf, treeNodeCompare) == NULL) {
            fprintf(stderr, "cannot allocate memory for linked list.  Quit.\n");
            freeHuffmanTree(leaf);
            freeListAndTrees(list);
            free(asciiCount);
            return EXIT_FAILURE;
         }
      }
   }

   if (!writeSortedOutput(argv[2], asciiCount)) {
      fprintf(stderr, "cannot open output file.  Quit.\n");
      freeListAndTrees(list);
      free(asciiCount);
      return EXIT_FAILURE;
   }

   if (list == NULL) {
      FILE *fp2 = fopen(argv[3], "w");
      FILE *fp3 = fopen(argv[4], "wb");
      if (fp2 == NULL || fp3 == NULL) {
         fprintf(stderr, "cannot open output file.  Quit.\n");
         if (fp2 != NULL) {
            fclose(fp2);
         }
         if (fp3 != NULL) {
            fclose(fp3);
         }
         free(asciiCount);
         return EXIT_FAILURE;
      }
      fclose(fp2);
      fclose(fp3);
      free(asciiCount);
      return EXIT_SUCCESS;
   }

   root = buildHuffmanTree(list);
   if (root == NULL) {
      fprintf(stderr, "cannot build huffman tree.  Quit.\n");
      free(asciiCount);
      return EXIT_FAILURE;
   }

   if (!writeHuffmanOutput(argv[3], root, asciiCount)) {
      fprintf(stderr, "cannot open output file.  Quit.\n");
      freeHuffmanTree(root);
      free(asciiCount);
      return EXIT_FAILURE;
   }

   if (!writeHeaderOutput(argv[4], root)) {
      fprintf(stderr, "cannot open output file.  Quit.\n");
      freeHuffmanTree(root);
      free(asciiCount);
      return EXIT_FAILURE;
   }

   freeHuffmanTree(root);
   free(asciiCount);

   return EXIT_SUCCESS;
}
