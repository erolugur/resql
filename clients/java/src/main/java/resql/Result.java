package resql;

import java.util.*;

class Result implements ResultSet, Iterator<Row> {

    private RawBuffer buf;
    private int linesChanged;
    private int nextResultSet;
    private int rowCount;
    private int remainingRows;
    private int columnCount;
    final Map<String, Integer> columnMap = new HashMap<>();
    final List<String> columnNames = new ArrayList<>();
    final List<Object> columnValues = new ArrayList<>();
    private final ResultRow row = new ResultRow();

    Result() {
    }

    void setBuf(RawBuffer buf) {
        columnMap.clear();
        columnNames.clear();
        columnValues.clear();

        this.buf = buf;
        nextResultSet = this.buf.position();

        nextResultSet();
    }

    @Override
    public int linesChanged() {
        return linesChanged;
    }

    @Override
    public int rowCount() {
        return rowCount;
    }

    @Override
    public boolean hasNext() {
        return remainingRows > 0;
    }

    @Override
    public Row next() {
        if (remainingRows-- <= 0) {
            return null;
        }

        columnValues.clear();

        for (int i = 0; i < columnCount; i++) {
            int param = buf.get();
            switch (param) {
                case Msg.PARAM_INTEGER:
                    columnValues.add(buf.getLong());
                    break;
                case Msg.PARAM_FLOAT:
                    columnValues.add(buf.getDouble());
                    break;
                case Msg.PARAM_TEXT:
                    columnValues.add(buf.getString());
                    break;
                case Msg.PARAM_BLOB:
                    columnValues.add(buf.getBlob());
                    break;
                case Msg.PARAM_NULL:
                    columnValues.add(null);
                    break;
                default:
                    throw new ResqlSQLException(
                            "Unexpected column type : " + param);
            }
        }

        return row;
    }

    @Override
    public boolean nextResultSet() {
        linesChanged = -1;
        rowCount = -1;
        columnCount = -1;
        remainingRows = -1;

        buf.position(nextResultSet);

        if (buf.get() != Msg.FLAG_STMT) {
            return false;
        }

        nextResultSet = buf.position() + buf.getInt();

        switch (buf.get()) {
            case Msg.FLAG_ROW:
                columnCount = buf.getInt();

                for (int i = 0; i < columnCount; i++) {
                    String columnName = buf.getString();
                    columnMap.put(columnName, i);
                    columnNames.add(columnName);
                }
                rowCount = buf.getInt();
                remainingRows = rowCount;
                break;
            case Msg.FLAG_DONE:
                linesChanged = buf.getInt();
                break;
            default:
                throw new ResqlException("Unexpected value");
        }

        return true;
    }

    @Override
    public Iterator<Row> iterator() {
        return this;
    }


    class ResultRow implements Row {

        ResultRow() {

        }

        public int columnCount() {
            return columnMap.size();
        }


        @Override
        @SuppressWarnings("unchecked")
        public <T> T get(String name)
        {
            Integer index = columnMap.get(name);
            if (index == null) {
                throw new ResqlSQLException(
                        "Column does not exists : " + name);
            }

            return (T) columnValues.get(index);
        }

        @Override
        @SuppressWarnings("unchecked")
        public <T> T get(int index) {
            try {
                return (T) columnValues.get(index);
            } catch (IndexOutOfBoundsException e) {
                throw new ResqlSQLException(e);
            }
        }

        @Override
        public String getColumnName(int index) {
            try {
                return columnNames.get(index);
            } catch (IndexOutOfBoundsException e) {
                throw new ResqlSQLException(e);
            }
        }
    }

}
