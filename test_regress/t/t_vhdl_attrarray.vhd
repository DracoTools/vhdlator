-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- "T'attr(i)" is one element of the value an attribute specification gave,
-- indexed by the array's own index (7.2, 8.4). The value is an expression
-- rather than an object, so it has no range of its own to index by, and the
-- index has to be brought back to a place in it: used as a bit number
-- straight, an ascending array reads from the wrong end. Where the
-- attribute's type is unconstrained the length is the aggregate's own
-- (9.3.3.3).

use std.env.all;

entity t_vhdl_attrarray is
end entity;

architecture tb of t_vhdl_attrarray is
  type mem is array (positive range <>) of bit;
  attribute up : mem;
  attribute up of mem : type is ('1', '0', '0', '0');
begin

  main: process is
  begin
    -- Ascending and unconstrained: the leftmost element is index 1
    assert mem'up(1) = '1' report "up(1) is " & bit'image(mem'up(1)) severity failure;
    assert mem'up(2) = '0' report "up(2) is " & bit'image(mem'up(2)) severity failure;
    assert mem'up(3) = '0' report "up(3) is " & bit'image(mem'up(3)) severity failure;
    assert mem'up(4) = '0' report "up(4) is " & bit'image(mem'up(4)) severity failure;

    report "*-* All Finished *-*" severity note;
    stop(0);
  end process;

end architecture;
