-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package pk is
  function f (i : bit) return integer;
end package;

package body pk is
  function f (i : bit) return integer is
  begin
    -- A name a package declares is not part of the design hierarchy, so its
    -- path starts at the library it was analyzed into (16.2.7)
    assert f'path_name = ":work:pk:f[bit return integer]:"
      report "f is " & f'path_name severity failure;
    assert f'instance_name = f'path_name severity failure;
    assert f'simple_name = "f" severity failure;
    return 0;
  end function;
end package body;

use work.pk.all;

entity t_vhdl_pathname is
end entity;

architecture arch of t_vhdl_pathname is
  signal a : bit;

  function g (i : bit) return integer is
  begin
    assert g'path_name = ":t_vhdl_pathname:g[bit return integer]:"
      report "g is " & g'path_name severity failure;
    -- 'INSTANCE_NAME names the architecture the region was built from
    assert g'instance_name = ":t_vhdl_pathname(arch):g[bit return integer]:"
      report "g instance is " & g'instance_name severity failure;
    return 0;
  end function;
begin

  gf : for i in 0 to 1 generate
    -- Which iteration this is belongs to the path (16.2.7)
    assert gf'path_name = ":t_vhdl_pathname:gf(" & integer'image(i) & "):"
      report "gf is " & gf'path_name severity failure;
  end generate;

  -- An unlabelled process has an empty name of its own, which the path shows
  -- as two colons together
  process is
    variable w : integer;
    variable v : bit := '1';
  begin
    assert w'path_name = ":t_vhdl_pathname::w"
      report "w is " & w'path_name severity failure;
    assert t_vhdl_pathname'path_name = ":t_vhdl_pathname:"
      report "unit is " & t_vhdl_pathname'path_name severity failure;
    assert f(v) = 0 severity failure;
    assert g(v) = 0 severity failure;
    wait;
  end process;

  blk : block
    signal b : bit;
  begin

    gi : if true generate
      signal c : bit;
    begin
      chk : process is
      begin
        -- The body of a generate is not a named region of its own: the
        -- generate statement's own label is what the path holds (16.2.7)
        assert a'path_name = ":t_vhdl_pathname:a"
          report "a is " & a'path_name severity failure;
        assert b'path_name = ":t_vhdl_pathname:blk:b"
          report "b is " & b'path_name severity failure;
        assert c'path_name = ":t_vhdl_pathname:blk:gi:c"
          report "c is " & c'path_name severity failure;
        assert c'instance_name = ":t_vhdl_pathname(arch):blk:gi:c"
          report "c instance is " & c'instance_name severity failure;
        assert chk'path_name = ":t_vhdl_pathname:blk:gi:chk:"
          report "chk is " & chk'path_name severity failure;
        assert gi'path_name = ":t_vhdl_pathname:blk:gi:"
          report "gi is " & gi'path_name severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
      end process;
    end generate;

  end block;

end architecture;
