-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An architecture continues its entity's declarative region (3.3, 12.1), so
-- a use clause the entity wrote is in scope there too, and a package the
-- entity declares or instantiates is reachable from it. A use clause naming
-- a package rather than one of its declarations makes the package's own
-- name visible (12.4), which an alias may then rename.

package shapes_pkg is
  generic ( type element_t );
  type pair_t is record
    a : element_t;
    b : element_t;
  end record;
  constant width : natural := 8;
end package;

package plain_pkg is
  constant tag : natural := 42;
end package;

use work.plain_pkg;

package alias_pkg is
  -- The package's own name, made visible by a use clause with no suffix
  alias renamed is plain_pkg;
end package;

use std.env.all;

entity t_vhdl_entvis is
  -- A package instantiated in the entity, and a use clause for it: both are
  -- in scope in the architecture below
  package int_shapes is new work.shapes_pkg generic map (integer);
  use int_shapes.all;
  constant here : natural := width;
end entity;

architecture tb of t_vhdl_entvis is
  -- "width" and "pair_t" come from the entity's use clause
  constant there : natural := width;
  signal p : pair_t := (a => 3, b => 4);
begin

  process is
  begin
    assert here = 8 report "entity" severity failure;
    assert there = 8 report "architecture" severity failure;
    assert p.a + p.b = 7 report "record" severity failure;
    -- And the package named by a use clause, renamed by an alias
    assert work.alias_pkg.renamed.tag = 42 report "alias" severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
