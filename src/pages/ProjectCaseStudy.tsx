import { useEffect, useState } from "react";
import { useParams, useNavigate } from "react-router-dom";
import { supabase } from "@/integrations/supabase/client";
import { Button } from "@/components/ui/button";
import { Badge } from "@/components/ui/badge";
import { Card } from "@/components/ui/card";
import { ArrowLeft, ExternalLink, Code, Calendar, Clock } from "lucide-react";
import Navigation from "@/components/Navigation";

interface Project {
  id: string;
  title: string;
  slug: string;
  description: string;
  tech_stack: string[];
  image_url: string | null;
  demo_url: string | null;
  code_url: string | null;
  featured: boolean;
  case_study_content: string | null;
  process_description: string | null;
  challenges: string | null;
  solutions: string | null;
  results: string | null;
  gallery_images: string[] | null;
  created_at: string;
}

const ProjectCaseStudy = () => {
  const { slug } = useParams();
  const navigate = useNavigate();
  const [project, setProject] = useState<Project | null>(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    if (slug) {
      fetchProject();
    }
  }, [slug]);

  const fetchProject = async () => {
    const { data, error } = await supabase
      .from("projects")
      .select("*")
      .eq("slug", slug)
      .single();

    if (error) {
      console.error("Error fetching project:", error);
    } else {
      setProject(data);
    }
    setLoading(false);
  };

  if (loading) {
    return (
      <div className="min-h-screen">
        <Navigation />
        <div className="container mx-auto px-4 pt-32 pb-12">
          <div className="text-center">Loading...</div>
        </div>
      </div>
    );
  }

  if (!project) {
    return (
      <div className="min-h-screen">
        <Navigation />
        <div className="container mx-auto px-4 pt-32 pb-12">
          <div className="text-center">
            <h1 className="text-4xl font-bold mb-4">Project Not Found</h1>
            <Button onClick={() => navigate("/#projects")}>
              <ArrowLeft className="mr-2 h-4 w-4" />
              Back to Projects
            </Button>
          </div>
        </div>
      </div>
    );
  }

  const formatDate = (dateString: string) => {
    return new Date(dateString).toLocaleDateString('en-US', {
      year: 'numeric',
      month: 'long',
      day: 'numeric'
    });
  };

  return (
    <div className="min-h-screen">
      <Navigation />
      <div className="container mx-auto px-4 pt-32 pb-12">
        <Button
          variant="ghost"
          onClick={() => navigate("/#projects")}
          className="mb-8"
        >
          <ArrowLeft className="mr-2 h-4 w-4" />
          Back to Projects
        </Button>

        <article className="max-w-6xl mx-auto">
          {/* Hero Section */}
          <div className="mb-12">
            {project.image_url && (
              <div className="aspect-video overflow-hidden rounded-lg mb-8">
                <img
                  src={project.image_url}
                  alt={project.title}
                  className="w-full h-full object-cover"
                />
              </div>
            )}

            <h1 className="text-5xl font-bold mb-4">
              <span className="text-gradient">{project.title}</span>
            </h1>

            <div className="flex items-center gap-6 text-muted-foreground mb-6 flex-wrap">
              <span className="flex items-center gap-2">
                <Calendar className="h-4 w-4" />
                {formatDate(project.created_at)}
              </span>
              {project.featured && (
                <Badge variant="secondary" className="bg-primary/10 text-primary">
                  Featured Project
                </Badge>
              )}
            </div>

            <p className="text-xl text-muted-foreground mb-8 leading-relaxed">
              {project.description}
            </p>

            {/* Tech Stack */}
            <div className="mb-8">
              <h3 className="text-lg font-semibold mb-4">Technologies Used</h3>
              <div className="flex flex-wrap gap-2">
                {project.tech_stack.map((tech, idx) => (
                  <Badge key={idx} variant="outline" className="text-sm">
                    {tech}
                  </Badge>
                ))}
              </div>
            </div>

            {/* Action Buttons */}
            <div className="flex gap-4 mb-12 flex-wrap">
              {project.demo_url && (
                <Button variant="hero" size="lg" asChild>
                  <a href={project.demo_url} target="_blank" rel="noopener noreferrer">
                    <ExternalLink className="mr-2 h-4 w-4" />
                    Live Demo
                  </a>
                </Button>
              )}
              {project.code_url && (
                <Button variant="outline" size="lg" asChild>
                  <a href={project.code_url} target="_blank" rel="noopener noreferrer">
                    <Code className="mr-2 h-4 w-4" />
                    View Code
                  </a>
                </Button>
              )}
            </div>
          </div>

          {/* Case Study Content */}
          {project.case_study_content && (
            <Card className="glass p-8 mb-8">
              <h2 className="text-3xl font-bold mb-6">Case Study</h2>
              <div 
                className="prose prose-lg max-w-none"
                dangerouslySetInnerHTML={{ __html: project.case_study_content }}
              />
            </Card>
          )}

          {/* Process, Challenges, Solutions Grid */}
          <div className="grid md:grid-cols-2 lg:grid-cols-3 gap-8 mb-8">
            {project.process_description && (
              <Card className="glass p-6">
                <h3 className="text-xl font-bold mb-4 text-primary">Process</h3>
                <div 
                  className="prose prose-sm max-w-none"
                  dangerouslySetInnerHTML={{ __html: project.process_description }}
                />
              </Card>
            )}

            {project.challenges && (
              <Card className="glass p-6">
                <h3 className="text-xl font-bold mb-4 text-orange-500">Challenges</h3>
                <div 
                  className="prose prose-sm max-w-none"
                  dangerouslySetInnerHTML={{ __html: project.challenges }}
                />
              </Card>
            )}

            {project.solutions && (
              <Card className="glass p-6">
                <h3 className="text-xl font-bold mb-4 text-green-500">Solutions</h3>
                <div 
                  className="prose prose-sm max-w-none"
                  dangerouslySetInnerHTML={{ __html: project.solutions }}
                />
              </Card>
            )}
          </div>

          {/* Results */}
          {project.results && (
            <Card className="glass p-8 mb-8">
              <h2 className="text-3xl font-bold mb-6">Results & Impact</h2>
              <div 
                className="prose prose-lg max-w-none"
                dangerouslySetInnerHTML={{ __html: project.results }}
              />
            </Card>
          )}

          {/* Gallery */}
          {project.gallery_images && project.gallery_images.length > 0 && (
            <Card className="glass p-8">
              <h2 className="text-3xl font-bold mb-6">Project Gallery</h2>
              <div className="grid md:grid-cols-2 lg:grid-cols-3 gap-4">
                {project.gallery_images.map((image, idx) => (
                  <div key={idx} className="aspect-video overflow-hidden rounded-lg">
                    <img
                      src={image}
                      alt={`${project.title} screenshot ${idx + 1}`}
                      className="w-full h-full object-cover hover:scale-105 transition-transform duration-300"
                    />
                  </div>
                ))}
              </div>
            </Card>
          )}
        </article>
      </div>
    </div>
  );
};

export default ProjectCaseStudy;
