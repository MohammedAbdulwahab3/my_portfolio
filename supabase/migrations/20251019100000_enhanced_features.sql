-- Enhanced Features Migration: Blog Enhancements, Project Case Studies, Testimonials

-- Add new columns to blog_posts for enhanced features
ALTER TABLE public.blog_posts ADD COLUMN IF NOT EXISTS tags text[] DEFAULT '{}';
ALTER TABLE public.blog_posts ADD COLUMN IF NOT EXISTS categories text[] DEFAULT '{}';
ALTER TABLE public.blog_posts ADD COLUMN IF NOT EXISTS reading_time integer DEFAULT 0;
ALTER TABLE public.blog_posts ADD COLUMN IF NOT EXISTS meta_description text;
ALTER TABLE public.blog_posts ADD COLUMN IF NOT EXISTS social_image text;

-- Add new columns to projects for case studies
ALTER TABLE public.projects ADD COLUMN IF NOT EXISTS slug text;
ALTER TABLE public.projects ADD COLUMN IF NOT EXISTS case_study_content text;
ALTER TABLE public.projects ADD COLUMN IF NOT EXISTS process_description text;
ALTER TABLE public.projects ADD COLUMN IF NOT EXISTS challenges text;
ALTER TABLE public.projects ADD COLUMN IF NOT EXISTS solutions text;
ALTER TABLE public.projects ADD COLUMN IF NOT EXISTS results text;
ALTER TABLE public.projects ADD COLUMN IF NOT EXISTS gallery_images text[] DEFAULT '{}';

-- Create unique constraint for project slugs (after adding the column)
CREATE UNIQUE INDEX IF NOT EXISTS projects_slug_unique ON public.projects(slug) WHERE slug IS NOT NULL;

-- Create testimonials table
CREATE TABLE IF NOT EXISTS public.testimonials (
  id uuid NOT NULL DEFAULT gen_random_uuid() PRIMARY KEY,
  name text NOT NULL,
  position text NOT NULL,
  company text NOT NULL,
  content text NOT NULL,
  avatar_url text,
  rating integer DEFAULT 5 CHECK (rating >= 1 AND rating <= 5),
  featured boolean DEFAULT false,
  created_at timestamp with time zone DEFAULT now(),
  updated_at timestamp with time zone DEFAULT now()
);

-- Create blog_comments table for comments system
CREATE TABLE IF NOT EXISTS public.blog_comments (
  id uuid NOT NULL DEFAULT gen_random_uuid() PRIMARY KEY,
  blog_post_id uuid REFERENCES public.blog_posts(id) ON DELETE CASCADE NOT NULL,
  author_name text NOT NULL,
  author_email text NOT NULL,
  content text NOT NULL,
  approved boolean DEFAULT false,
  created_at timestamp with time zone DEFAULT now(),
  updated_at timestamp with time zone DEFAULT now()
);

-- Create blog_tags table for better tag management
CREATE TABLE IF NOT EXISTS public.blog_tags (
  id uuid NOT NULL DEFAULT gen_random_uuid() PRIMARY KEY,
  name text NOT NULL UNIQUE,
  slug text NOT NULL UNIQUE,
  description text,
  color text DEFAULT '#3B82F6',
  created_at timestamp with time zone DEFAULT now()
);

-- Create blog_categories table for better category management
CREATE TABLE IF NOT EXISTS public.blog_categories (
  id uuid NOT NULL DEFAULT gen_random_uuid() PRIMARY KEY,
  name text NOT NULL UNIQUE,
  slug text NOT NULL UNIQUE,
  description text,
  color text DEFAULT '#10B981',
  created_at timestamp with time zone DEFAULT now()
);

-- Enable RLS on new tables
ALTER TABLE public.testimonials ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.blog_comments ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.blog_tags ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.blog_categories ENABLE ROW LEVEL SECURITY;

-- RLS Policies for testimonials
CREATE POLICY "Anyone can view testimonials"
ON public.testimonials
FOR SELECT
USING (true);

CREATE POLICY "Admins can insert testimonials"
ON public.testimonials
FOR INSERT
WITH CHECK (has_role(auth.uid(), 'admin'::app_role));

CREATE POLICY "Admins can update testimonials"
ON public.testimonials
FOR UPDATE
USING (has_role(auth.uid(), 'admin'::app_role));

CREATE POLICY "Admins can delete testimonials"
ON public.testimonials
FOR DELETE
USING (has_role(auth.uid(), 'admin'::app_role));

-- RLS Policies for blog_comments
CREATE POLICY "Anyone can view approved comments"
ON public.blog_comments
FOR SELECT
USING (approved = true OR has_role(auth.uid(), 'admin'::app_role));

CREATE POLICY "Anyone can insert comments"
ON public.blog_comments
FOR INSERT
WITH CHECK (true);

CREATE POLICY "Admins can update comments"
ON public.blog_comments
FOR UPDATE
USING (has_role(auth.uid(), 'admin'::app_role));

CREATE POLICY "Admins can delete comments"
ON public.blog_comments
FOR DELETE
USING (has_role(auth.uid(), 'admin'::app_role));

-- RLS Policies for blog_tags
CREATE POLICY "Anyone can view tags"
ON public.blog_tags
FOR SELECT
USING (true);

CREATE POLICY "Admins can manage tags"
ON public.blog_tags
FOR ALL
USING (has_role(auth.uid(), 'admin'::app_role));

-- RLS Policies for blog_categories
CREATE POLICY "Anyone can view categories"
ON public.blog_categories
FOR SELECT
USING (true);

CREATE POLICY "Admins can manage categories"
ON public.blog_categories
FOR ALL
USING (has_role(auth.uid(), 'admin'::app_role));

-- Triggers for updated_at timestamps
CREATE TRIGGER update_testimonials_updated_at
BEFORE UPDATE ON public.testimonials
FOR EACH ROW
EXECUTE FUNCTION public.update_updated_at_column();

CREATE TRIGGER update_blog_comments_updated_at
BEFORE UPDATE ON public.blog_comments
FOR EACH ROW
EXECUTE FUNCTION public.update_updated_at_column();

-- Function to calculate reading time
CREATE OR REPLACE FUNCTION public.calculate_reading_time(content_text text)
RETURNS integer
LANGUAGE plpgsql
AS $$
DECLARE
  word_count integer;
  reading_time integer;
BEGIN
  -- Count words (rough estimation)
  word_count := array_length(string_to_array(trim(content_text), ' '), 1);
  -- Average reading speed: 200 words per minute
  reading_time := CEIL(word_count::float / 200.0);
  -- Minimum 1 minute
  RETURN GREATEST(reading_time, 1);
END;
$$;

-- Function to auto-update reading time on blog posts
CREATE OR REPLACE FUNCTION public.update_blog_reading_time()
RETURNS TRIGGER
LANGUAGE plpgsql
AS $$
BEGIN
  NEW.reading_time := public.calculate_reading_time(NEW.content);
  RETURN NEW;
END;
$$;

-- Trigger to auto-calculate reading time
CREATE TRIGGER calculate_blog_reading_time
BEFORE INSERT OR UPDATE ON public.blog_posts
FOR EACH ROW
EXECUTE FUNCTION public.update_blog_reading_time();

-- Insert some sample tags and categories
INSERT INTO public.blog_tags (name, slug, description, color) VALUES
('React', 'react', 'JavaScript library for building user interfaces', '#61DAFB'),
('TypeScript', 'typescript', 'Typed superset of JavaScript', '#3178C6'),
('Flutter', 'flutter', 'Google''s UI toolkit for mobile development', '#02569B'),
('Firebase', 'firebase', 'Google''s mobile and web application development platform', '#FFCA28'),
('Django', 'django', 'High-level Python web framework', '#092E20'),
('Golang', 'golang', 'Open source programming language', '#00ADD8')
ON CONFLICT (slug) DO NOTHING;

INSERT INTO public.blog_categories (name, slug, description, color) VALUES
('Web Development', 'web-development', 'Articles about web development', '#3B82F6'),
('Mobile Development', 'mobile-development', 'Articles about mobile app development', '#10B981'),
('Backend Development', 'backend-development', 'Articles about server-side development', '#8B5CF6'),
('DevOps', 'devops', 'Articles about development operations', '#F59E0B'),
('Tutorials', 'tutorials', 'Step-by-step guides and tutorials', '#EF4444'),
('Career', 'career', 'Career advice and insights', '#6366F1')
ON CONFLICT (slug) DO NOTHING;
