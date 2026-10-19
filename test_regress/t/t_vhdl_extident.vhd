-- DESCRIPTION: Verilator: VHDL extended identifiers that differ only in case, or from a basic one (NVC issue816)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

-- 15.4.3: an extended identifier is case sensitive and distinct from every
-- basic identifier, so these are three entities and three signals

entity \foo\ is
  port (x : out integer);
end entity;
architecture \test\ of \foo\ is
begin
  x <= 1;
end architecture;

entity \Foo\ is
  port (x : out integer);
end entity;
architecture \test\ of \Foo\ is
begin
  x <= 2;
end architecture;

entity foo is
  port (x : out integer);
end entity;
architecture test of foo is
begin
  x <= 3;
end architecture;

entity t_vhdl_extident is
end entity;

architecture a of t_vhdl_extident is
  signal a, b, c : integer;
  signal \Bus\, \bus\, bus_s : integer := 0;
  type enum is (lo, \Lo\, \lo\);
begin
  u1 : entity work.\foo\ port map (a);
  u2 : entity work.\Foo\ port map (b);
  u3 : entity work.foo port map (c);

  process is
    variable e : enum := \Lo\;
  begin
    \Bus\ <= 10;
    \bus\ <= 20;
    bus_s <= 30;
    wait for 1 ns;
    assert a = 1 severity failure;
    assert b = 2 severity failure;
    assert c = 3 severity failure;
    assert \Bus\ = 10 and \bus\ = 20 and bus_s = 30 severity failure;
    assert enum'pos(e) = 1 severity failure;
    assert enum'image(lo) = "lo" severity failure;
    assert enum'image(e) = "\Lo\" severity failure;
    assert enum'image(\lo\) = "\lo\" severity failure;
    assert to_string(e) = "Lo" severity failure;
    assert enum'value("\lo\") = \lo\ severity failure;
    assert enum'value("LO") = lo severity failure;
    assert \Bus\'simple_name = "\Bus\" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
