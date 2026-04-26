#pragma once

#include "combinetool/merge/merger.h"
#include "combinetool/filter/filter.h"

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
    
    std::unique_ptr<filter::Filter> m_filter;
    std::unique_ptr<filter::Deduplicator> m_deduplicator;
    
    struct FileData {
        std::string filePath;
        std::vector<std::vector<std::string>> rows;
        std::vector<std::string> headers;
        int keyColumnIndex;
    };
    
    std::vector<FileData> m_fileData;
    std::unique_ptr<std::ofstream> m_outputStream;
    
    bool loadAllFiles();
    bool resolveKeyColumns();
    bool performJoin();
    
    std::vector<std::string> combineRows(
        const std::vector<std::string>& row1,
        const std::vector<std::string>& row2,
        size_t file1Cols,
        size_t file2Cols
    );
    
    bool writeLine(const std::string& line);
};

}
}
