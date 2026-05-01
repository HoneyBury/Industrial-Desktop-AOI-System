# LaserSpc Ingest API

`LaserSpcIngestServer` provides write APIs for upstream machines to push SPC source data into MySQL.

Base URL:

```text
http://<host>:8099
```

Endpoints:

```text
GET  /health
POST /api/v1/board-records
POST /api/v1/point-records
POST /api/v1/inspection-batches
```

Auth:

```text
Header: X-API-Key: <your key>
```

If `LASERSPC_API_KEY` is empty, auth is disabled. If set, every `POST` request must carry the header.

`POST /api/v1/board-records`

```json
{
  "boardCode": "BD-240301-0101",
  "result": "OK",
  "lineName": "L1",
  "programName": "Program-A",
  "deviceName": "Laser-01",
  "operatorName": "Alice",
  "eventTime": "2026-03-16T10:15:30"
}
```

`POST /api/v1/point-records`

```json
{
  "boardCode": "BD-240301-0101",
  "pointName": "Code-A1",
  "result": "OK",
  "readGrade": "A",
  "lineName": "L1",
  "programName": "Program-A",
  "deviceName": "Laser-01",
  "startTime": "2026-03-16T10:15:00",
  "endTime": "2026-03-16T10:15:30"
}
```

`POST /api/v1/inspection-batches`

```json
{
  "requestId": "batch-20260316-001",
  "board": {
    "boardCode": "BD-240301-0101",
    "result": "NG",
    "lineName": "L1",
    "programName": "Program-A",
    "deviceName": "Laser-01",
    "operatorName": "Alice",
    "eventTime": "2026-03-16T10:15:30"
  },
  "points": [
    {
      "pointName": "Code-A1",
      "result": "OK",
      "readGrade": "A",
      "startTime": "2026-03-16T10:15:00",
      "endTime": "2026-03-16T10:15:10"
    },
    {
      "pointName": "Code-A2",
      "result": "NG",
      "readGrade": "C",
      "startTime": "2026-03-16T10:15:11",
      "endTime": "2026-03-16T10:15:30"
    }
  ]
}
```

Behavior:

- `board-records` uses upsert semantics on `board_code`.
- `point-records` inserts one point row.
- `inspection-batches` upserts the board row, deletes old points for that board, then inserts the new point set in one transaction.
- If MES is enabled in current app config, `inspection-batches` will also forward the batch payload to `mes.endpointUrl` after database write succeeds.
- MES forward failure will be returned in `forwardMessage`, but the database write remains committed.

Environment variables:

```text
LASERSPC_API_HOST=0.0.0.0
LASERSPC_API_PORT=8099
LASERSPC_API_KEY=replace-with-your-token
LASERSPC_DB_HOST=127.0.0.1
LASERSPC_DB_PORT=3306
LASERSPC_DB_NAME=laser_spc
LASERSPC_DB_USER=laserspc
LASERSPC_DB_PASSWORD=LaserSpc#2026
```
