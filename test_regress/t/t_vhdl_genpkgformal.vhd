-- DESCRIPTION: Verilator: VHDL generic package whose formal generic package
-- is mapped to an instance of the same name
--
-- A generic package takes another as a formal generic package ("generic
-- map (<>)") and makes it visible with a use clause. An architecture
-- instantiates the first and then the second, mapping the formal to that
-- instance -- which has the formal's own name. Inside the second instance
-- the use clause reaches the first instance's declarations and nothing
-- else; the uninstantiated package's own are not in view (4.9) -- GHDL gna
-- issue476.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_genpkgformal_pix is
  generic (NBITS : natural; NCHROMA : natural);
  subtype chroma_t is bit_vector(NBITS - 1 downto 0);
  type pixel_t is array (NCHROMA - 1 downto 0) of chroma_t;
  function invert (p : pixel_t) return pixel_t;
end package;

package body t_vhdl_genpkgformal_pix is
  function invert (p : pixel_t) return pixel_t is
    variable r : pixel_t;
  begin
    for i in p'range loop
      r(i) := not p(i);
    end loop;
    return r;
  end function;
end package body;

use work.t_vhdl_genpkgformal_pix;

package t_vhdl_genpkgformal_col is
  generic (NROW : natural;
           package local_pix is new t_vhdl_genpkgformal_pix generic map (<>));
  use local_pix.all;
  type column_t is array (NROW - 1 downto 0) of pixel_t;
  function invert_column (c : column_t) return column_t;
end package;

package body t_vhdl_genpkgformal_col is
  function invert_column (c : column_t) return column_t is
    variable r : column_t;
  begin
    for i in c'range loop
      r(i) := invert(c(i));
    end loop;
    return r;
  end function;
end package body;

entity t_vhdl_genpkgformal is
end entity;

architecture test of t_vhdl_genpkgformal is
  package local_pix is new work.t_vhdl_genpkgformal_pix generic map (NBITS => 4, NCHROMA => 2);
  package local_col is new work.t_vhdl_genpkgformal_col
    generic map (NROW => 2, local_pix => local_pix);
  use local_pix.all;
  use local_col.all;
  signal c, r : column_t;
begin
  r <= invert_column(c);

  process is
  begin
    c <= (1 => ("0001", "0010"), 0 => ("1100", "0000"));
    wait for 1 ns;
    assert r(1) = ("1110", "1101") and r(0) = ("0011", "1111") severity failure;
    assert r(1)(0)'length = 4 severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
