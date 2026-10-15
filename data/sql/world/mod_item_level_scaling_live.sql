--
-- mod-twow-item-level-scaling: Unified Synchronous Live Generation Schema Cleanup
-- Staged and slot tables are deprecated in favor of direct live generation into scaled_item_variant and item_template.
--

DROP TABLE IF EXISTS `mod_item_level_scaling_slot`;
DROP TABLE IF EXISTS `mod_item_level_scaling_staged_item`;
DROP TABLE IF EXISTS `mod_item_level_scaling_staged_variant`;
