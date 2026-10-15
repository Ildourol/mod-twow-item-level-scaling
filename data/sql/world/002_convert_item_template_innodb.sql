--
-- Convert item_template to InnoDB storage engine for mod-twow-item-level-scaling
-- Provides row-level locking, high concurrency, and transactional stability
--

ALTER TABLE `item_template` ENGINE = InnoDB;
