-- DevTools audit log: records every mutating call made through the admin API
-- so operators can see who did what, when, from where, and with what payload.
-- +migrate Up
CREATE TABLE IF NOT EXISTS devtoolsAudit (
    auditID     BIGINT(20)   NOT NULL AUTO_INCREMENT,
    ts          DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    accountID   INT(10)      NOT NULL DEFAULT 0,
    accountName VARCHAR(64)  NOT NULL DEFAULT '',
    remoteAddr  VARCHAR(64)  NOT NULL DEFAULT '',
    method      VARCHAR(8)   NOT NULL DEFAULT '',
    path        VARCHAR(512) NOT NULL DEFAULT '',
    status      SMALLINT(5)  NOT NULL DEFAULT 0,
    requestBody MEDIUMTEXT,
    notes       VARCHAR(512) NOT NULL DEFAULT '',
    PRIMARY KEY (auditID),
    KEY idx_devtoolsAudit_ts (ts),
    KEY idx_devtoolsAudit_account (accountID)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_bin;

-- +migrate Down
DROP TABLE IF EXISTS devtoolsAudit;
