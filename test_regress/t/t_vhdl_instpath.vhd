-- DESCRIPTION: Verilator: VHDL 'PATH_NAME and 'INSTANCE_NAME through instances
--
-- This file is part of Verilator.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
--
-- A path names every region from the root of the design down (16.2.7): an
-- object inside an instantiated entity is named through the instances it is
-- reached by, and so differs from one instance to the next. VUnit's
-- verification components seed their random generators with
-- rnd'instance_name, so two instances drew the same numbers. nvc 1.22.1 and
-- GHDL 7.0.0-dev print every string checked here.

entity t_vhdl_instpath_leaf is
  generic (exp_path : string := ":t_vhdl_instpath_leaf";
           exp_inst : string := ":t_vhdl_instpath_leaf(rtl)");
end entity;

architecture rtl of t_vhdl_instpath_leaf is
  signal s : bit;
  function f return integer is
  begin
    return 1;
  end function;
begin
  p : process
    variable v : integer;
    -- The path of a subprogram, up to where nvc and GHDL write its profile
    -- differently
    function head(str : string; n : natural) return string is
    begin
      return str(str'left to str'left + n - 1);
    end function;
  begin
    report s'path_name & " " & s'instance_name;
    assert s'path_name = exp_path & ":s" report "s'path_name" severity failure;
    assert s'instance_name = exp_inst & "@t_vhdl_instpath_leaf(rtl):s"
      report "s'instance_name" severity failure;
    assert v'path_name = exp_path & ":p:v" report "v'path_name" severity failure;
    assert v'instance_name = exp_inst & "@t_vhdl_instpath_leaf(rtl):p:v"
      report "v'instance_name" severity failure;
    -- A generic is declared by the entity, not the architecture
    assert exp_path'path_name = exp_path & ":exp_path" report "generic 'path_name" severity failure;
    assert exp_inst'instance_name = exp_inst & "@t_vhdl_instpath_leaf(rtl):exp_inst"
      report "generic 'instance_name" severity failure;
    assert head(f'path_name, exp_path'length + 3) = exp_path & ":f["
      report "f'path_name" severity failure;
    wait;
  end process;
end architecture;

entity t_vhdl_instpath_mid is
  generic (exp_path : string; exp_inst : string);
end entity;

architecture rtl of t_vhdl_instpath_mid is
begin
  ul : entity work.t_vhdl_instpath_leaf
    generic map (exp_path => exp_path & ":ul",
                 exp_inst => exp_inst & "@t_vhdl_instpath_mid(rtl):ul");
end architecture;

entity t_vhdl_instpath is
end entity;

architecture tb of t_vhdl_instpath is
  signal ts : bit;
begin
  u1 : entity work.t_vhdl_instpath_leaf
    generic map (exp_path => ":t_vhdl_instpath:u1", exp_inst => ":t_vhdl_instpath(tb):u1");

  g : for i in 0 to 1 generate
    um : entity work.t_vhdl_instpath_mid
      generic map (exp_path => ":t_vhdl_instpath:g(" & integer'image(i) & "):um",
                   exp_inst => ":t_vhdl_instpath(tb):g(" & integer'image(i) & "):um");
  end generate;

  b : block
  begin
    u3 : entity work.t_vhdl_instpath_leaf
      generic map (exp_path => ":t_vhdl_instpath:b:u3", exp_inst => ":t_vhdl_instpath(tb):b:u3");
  end block;

  process
  begin
    assert ts'path_name = ":t_vhdl_instpath:ts" report "ts'path_name" severity failure;
    assert ts'instance_name = ":t_vhdl_instpath(tb):ts" report "ts'instance_name" severity failure;
    wait for 1 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
