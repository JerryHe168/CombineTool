#pragma once

#include "combinetool/merge/merger.h"
#include "combinetool/io/smart_file_reader.h"

#include <map>
#include <set>
#include <unordered_map>
#include <vector>
#include <string>

namespace combinetool {
namespace merge {

class ConditionalMerger : public Merger {
public:
    explicit ConditionalMerger(const MergeConfig& config);
    ~ConditionalMerger() override;
    
    bool merge() override;
    
    void setJoinKeyColumn(int columnIndex);
    void setJoinKeyColumn(const std::string& columnName);
    
    void setJoinType(const std::string& joinType);

private:
    enum class JoinType {
        Inner,
        Left,
        Right,
        Full
    };
    
    int m_joinKeyColumn;
    std::string m_joinKeyColumnName;
    JoinType m_joinType;
    
    struct FileInfo {
        std::string filePath;
        std::vector<std::string> headers;
        int keyColumnIndex;
        size_t columnCount;
        std::string delimiter;
        bool hasHeader;
    };
    
    using RowIndexMap = std::unordered_map<std::string, std::vector<size_t>>;
    using RowList = std::vector<std::vector<std::string>>;
    
    std::vector<FileInfo> m_fileInfos;
    std::unique_ptr<std::ofstream> m_outputStream;
    
    bool prepareFiles();
    bool resolveKeyColumns();
    bool performHashJoin();
    
    bool loadRightTableToMemory(
        const std::string& filePath,
        const std::string& delimiter,
        bool hasHeader,
        size_t keyColumnIndex,
        RowList& rows,
        RowIndexMap& index,
        size_t& columnCount
    );
    
    bool streamLeftTableAndJoin(
        const std::string& filePath,
        const std::string& delimiter,
        bool hasHeader,
        size_t leftKeyColumnIndex,
        size_t leftColumnCount,
        const RowList& rightRows,
        const RowIndexMap& rightIndex,
        size_t rightColumnCount,
        const std::vector<std::string>& rightHeaders,
        bool writeHeaders
    );
    
    std::vector<std::string> combineRows(
        const std::vector<std::string>& row1,
        const std::vector<std::string>& row2,
        size_t file1Cols,
        size_t file2Cols
    );
    
    bool writeLine(const std::string& line);
    bool writeEmptyRow(size_t columnCount);
    
    bool shouldUseHashJoin(uint64_t file1Size, uint64_t file2Size) const;
};

}
}
