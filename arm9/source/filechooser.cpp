#include <nds.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <vector>
#include "filechooser.h"
#include "inputhelper.h"

const int filesPerPage = 23;
const int MAX_FILENAME_LEN = 100;
int numFiles;
int scrollY=0;
int fileSelection=0;

void updateScrollDown() {
    if (fileSelection >= numFiles)
        fileSelection = numFiles-1;
    if (fileSelection == numFiles-1 && numFiles > filesPerPage)
        scrollY = fileSelection-filesPerPage+1;
    else if (fileSelection-scrollY >= filesPerPage-1)
        scrollY = fileSelection-filesPerPage+2;
}
void updateScrollUp() {
    if (fileSelection < 0)
        fileSelection = 0;
    if (fileSelection == 0)
        scrollY = 0;
    else if (fileSelection == scrollY)
        scrollY--;
    else if (fileSelection < scrollY)
        scrollY = fileSelection-1;

}

int nameSortFunction(char*& a, char*& b)
{
    // ".." sorts before everything except itself.
    bool aIsParent = strcmp(a, "..") == 0;
    bool bIsParent = strcmp(b, "..") == 0;

    if (aIsParent && bIsParent)
        return 0;
    else if (aIsParent) // Sorts before
        return -1;
    else if (bIsParent) // Sorts after
        return 1;
    else
        return strcmp(a, b);
}

/*
 * Determines whether a portion of a vector is sorted.
 * Input assertions: 'from' and 'to' are valid indices into data. 'to' can be
 *   the maximum value for the type 'unsigned int'.
 * Input: 'data', data vector, possibly sorted.
 *        'sortFunction', function determining the sort order of two elements.
 *        'from', index of the first element in the range to test.
 *        'to', index of the last element in the range to test.
 * Output: true if, for any valid index 'i' such as from <= i < to,
 *   data[i] < data[i + 1].
 *   true if the range is one or no elements, or if from > to.
 *   false otherwise.
 */
template <class Data> bool isSorted(std::vector<Data>& data, int (*sortFunction) (Data&, Data&), const unsigned int from, const unsigned int to)
{
    if (from >= to)
        return true;

    Data* prev = &data[from];
    for (unsigned int i = from + 1; i < to; i++)
    {
        if ((*sortFunction)(*prev, data[i]) > 0)
            return false;
        prev = &data[i];
    }
    if ((*sortFunction)(*prev, data[to]) > 0)
        return false;

    return true;
}

/*
 * Chooses a pivot for Quicksort. Uses the median-of-three search algorithm
 * first proposed by Robert Sedgewick.
 * Input assertions: 'from' and 'to' are valid indices into data. 'to' can be
 *   the maximum value for the type 'unsigned int'.
 * Input: 'data', data vector.
 *        'sortFunction', function determining the sort order of two elements.
 *        'from', index of the first element in the range to be sorted.
 *        'to', index of the last element in the range to be sorted.
 * Output: a valid index into data, between 'from' and 'to' inclusive.
 */
template <class Data> unsigned int choosePivot(std::vector<Data>& data, int (*sortFunction) (Data&, Data&), const unsigned int from, const unsigned int to)
{
    // The highest of the two extremities is calculated first.
    unsigned int highest = ((*sortFunction)(data[from], data[to]) > 0)
        ? from
        : to;
    // Then the lowest of that highest extremity and the middle
    // becomes the pivot.
    return ((*sortFunction)(data[from + (to - from) / 2], data[highest]) < 0)
        ? (from + (to - from) / 2)
        : highest;
}

/*
 * Partition function for Quicksort. Moves elements such that those that are
 * less than the pivot end up before it in the data vector.
 * Input assertions: 'from', 'to' and 'pivotIndex' are valid indices into data.
 *   'to' can be the maximum value for the type 'unsigned int'.
 * Input: 'data', data vector.
 *        'metadata', data describing the values in 'data'.
 *        'sortFunction', function determining the sort order of two elements.
 *        'from', index of the first element in the range to sort.
 *        'to', index of the last element in the range to sort.
 *        'pivotIndex', index of the value chosen as the pivot.
 * Output: the index of the value chosen as the pivot after it has been moved
 *   after all the values that are less than it.
 */
template <class Data, class Metadata> unsigned int partition(std::vector<Data>& data, std::vector<Metadata>& metadata, int (*sortFunction) (Data&, Data&), const unsigned int from, const unsigned int to, const unsigned int pivotIndex)
{
    Data pivotValue = data[pivotIndex];
    data[pivotIndex] = data[to];
    data[to] = pivotValue;
    {
        const Metadata tM = metadata[pivotIndex];
        metadata[pivotIndex] = metadata[to];
        metadata[to] = tM;
    }

    unsigned int storeIndex = from;
    for (unsigned int i = from; i < to; i++)
    {
        if ((*sortFunction)(data[i], pivotValue) < 0)
        {
            const Data tD = data[storeIndex];
            data[storeIndex] = data[i];
            data[i] = tD;
            const Metadata tM = metadata[storeIndex];
            metadata[storeIndex] = metadata[i];
            metadata[i] = tM;
            ++storeIndex;
        }
    }

    {
        const Data tD = data[to];
        data[to] = data[storeIndex];
        data[storeIndex] = tD;
        const Metadata tM = metadata[to];
        metadata[to] = metadata[storeIndex];
        metadata[storeIndex] = tM;
    }
    return storeIndex;
}

/*
 * Sorts an array while keeping metadata in sync.
 * This sort is unstable and its average performance is
 *   O(data.size() * log2(data.size()).
 * Input assertions: for any valid index 'i' in data, index 'i' is valid in
 *   metadata. 'from' and 'to' are valid indices into data. 'to' can be
 *   the maximum value for the type 'unsigned int'.
 * Invariant: index 'i' in metadata describes index 'i' in data.
 * Input: 'data', data to sort.
 *        'metadata', data describing the values in 'data'.
 *        'sortFunction', function determining the sort order of two elements.
 *        'from', index of the first element in the range to sort.
 *        'to', index of the last element in the range to sort.
 */
template <class Data, class Metadata> void quickSort(std::vector<Data>& data, std::vector<Metadata>& metadata, int (*sortFunction) (Data&, Data&), const unsigned int from, const unsigned int to)
{
    if (isSorted(data, sortFunction, from, to))
        return;

    unsigned int pivotIndex = choosePivot(data, sortFunction, from, to);
    unsigned int newPivotIndex = partition(data, metadata, sortFunction, from, to, pivotIndex);
    if (newPivotIndex > 0)
        quickSort(data, metadata, sortFunction, from, newPivotIndex - 1);
    if (newPivotIndex < to)
        quickSort(data, metadata, sortFunction, newPivotIndex + 1, to);
}

/*
 * Prompts the user for a file to load.
 * Returns a pointer to a newly-allocated string. The caller is responsible
 * for free()ing it.
 */
char* startFileChooser() {
    char* retval;
    char cwd[256];
    getcwd(cwd, 256);
    DIR* dp = opendir(cwd);
    struct dirent *entry;
    if (dp == NULL) {
        iprintf("Error opening directory.\n");
        return 0;
    }
    while (true) {
        numFiles=0;
        std::vector<char*> filenames;
        std::vector<bool> directory;
        while ((entry = readdir(dp)) != NULL) {
            char* ext = strrchr(entry->d_name, '.')+1;
            if (entry->d_type & DT_DIR || strcmpi(ext, "cgb") == 0 || strcmpi(ext, "gbc") == 0 || strcmpi(ext, "gb") == 0 || strcmpi(ext, "sgb") == 0 ||
					strcmpi(ext, "gbs") == 0) {
                if (!(strcmp(".", entry->d_name) == 0)) {
                    directory.push_back(entry->d_type & DT_DIR);
                    char *name = (char*)malloc(sizeof(char)*(strlen(entry->d_name)+1));
                    strcpy(name, entry->d_name);
                    filenames.push_back(name);
                    numFiles++;
                }
            }
        }

        quickSort(filenames, directory, nameSortFunction, 0, numFiles - 1);

        scrollY=0;
        updateScrollDown();
        bool readDirectory = false;
        while (!readDirectory) {
            consoleClear();
            for (int i=scrollY; i<scrollY+filesPerPage && i<numFiles; i++) {
                if (i == fileSelection)
                    iprintf("* ");
                else if (i == scrollY && i != 0)
                    iprintf("^ ");
                else if (i == scrollY+filesPerPage-1 && scrollY+filesPerPage-1 != numFiles-1)
                    iprintf("v ");
                else
                    iprintf("  ");
                char outBuffer[32];

                int maxLen = 29;
                if (directory[i])
                    maxLen--;
                strncpy(outBuffer, filenames[i], maxLen);
                outBuffer[maxLen] = '\0';
                if (directory[i])
                    iprintf("%s/\n", outBuffer);
                else
                    iprintf("%s\n", outBuffer);
            }

            while (true) {
                swiWaitForVBlank();
                readKeys();
                if (keyJustPressed(KEY_A)) {
                    if (directory[fileSelection]) {
                        closedir(dp);
                        dp = opendir(filenames[fileSelection]);
                        chdir(filenames[fileSelection]);
                        readDirectory = true;
                        break;
                    }
                    else {
                        // Copy the result to a new allocation, as the
                        // filename would become unavailable when freed.
                        retval = (char*) malloc(sizeof(char) * (strlen(filenames[fileSelection]) + 1));
                        strcpy(retval, filenames[fileSelection]);
                        // free memory used for filenames in this dir
                        for (int i=0; i<numFiles; i++) {
                            free(filenames[i]);
                        }
                        goto end;
                    }
                }
                else if (keyJustPressed(KEY_B)) {
                    if (numFiles >= 1 && strcmp(filenames[0], "..") == 0) {
                        closedir(dp);
                        dp = opendir("..");
                        chdir("..");
                        readDirectory = true;
                        break;
                    }
                }
                else if (keyPressedAutoRepeat(KEY_UP)) {
                    if (fileSelection > 0) {
                        fileSelection--;
                        updateScrollUp();
                        break;
                    }
                }
                else if (keyPressedAutoRepeat(KEY_DOWN)) {
                    if (fileSelection < numFiles-1) {
                        fileSelection++;
                        updateScrollDown();
                        break;
                    }
                }
                else if (keyPressedAutoRepeat(KEY_RIGHT)) {
                    fileSelection += filesPerPage/2;
                    updateScrollDown();
                    break;
                }
                else if (keyPressedAutoRepeat(KEY_LEFT)) {
                    fileSelection -= filesPerPage/2;
                    updateScrollUp();
                    break;
                }
            }
        }
        // free memory used for filenames
        for (int i=0; i<numFiles; i++) {
            free(filenames[i]);
        }
        fileSelection = 0;
    }
end:
    closedir(dp);
    consoleClear();
    return retval;
}
