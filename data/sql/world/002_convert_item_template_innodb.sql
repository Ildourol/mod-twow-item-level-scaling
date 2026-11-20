--
-- Convert item_template to InnoDB storage engine for mod-twow-item-level-scaling
-- Provides row-level locking, high concurrency, and transactional stability
-- Stock TortoiseWoW uses MyISAM with ROW_FORMAT=FIXED, which InnoDB cannot use.
--

ALTER TABLE `item_template` ENGINE = InnoDB ROW_FORMAT = DYNAMIC;
